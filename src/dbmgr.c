#include <dbmgr.h>
#include "dbmgr_impl.h"
#include "dbmgr_info_get.h"
#include <scheduler.h>

#include <msg_test.h>
#include <msg_test.pb-c.h>
#include <msg_readcdb.h>
#include <msg_readcdb.pb-c.h>
#ifdef HAVE_VNFHA
#include <vnf_hactrl_config.h>
#endif
#include <db_api_sync.h>
#include <db_cdb.h>
#include <db_common.h>
#include <record_cdb.h>
#include <record_cdb.pb-c.h>
#include <record_common.h>
#include <msg_dbmgr.h>
#include <msg_dbmgr.pb-c.h>
#include <glib.h>
#include <stdio.h>
#include <db_ha.h>

#ifdef HAVE_VNFHA
extern char* vnf_id_g; //vnf id
#endif

static DBMgr *dbmgr_new(ltegw_dbmgr_msg_handle_t * dbmgr_show_msg);
static void dbmgr_free(DBMgr *mgr);
static void dbmgr_msg_evt_callback(void *arg, unsigned event, void *data);
static void dbmgr_readcdb_vnf_ha_request_handler(MSGHandle *mh, void *arg);

static void ha_get_count_request_handler(MSGHandle *mh, void *arg)
{
    DbmgrTotalCountResponse rsp = MSG_INIT(DBMGR,  TOTAL_COUNT_RESPONSE);    
    ltegw_dbmgr_msg_handle_t * show_msg = (ltegw_dbmgr_msg_handle_t *)arg;
    int ret = 0;
    if (show_msg->dbmgr_show_state == LTEGW_DBMGR_BUZY)
    {
        rsp.dbmgr_error = -10;
        msgapi_send_reply(mh, MSGID(DBMGR,  TOTAL_COUNT_RESPONSE), &rsp);
        return;
    }
    memset(show_msg, 0, sizeof(ltegw_dbmgr_msg_handle_t));
    
    show_msg->dbmgr_show_state = LTEGW_DBMGR_BUZY;
    show_msg->msg_type = LTEGW_DBMGR_MSG_GET_HA_IPSEC_TOTAL_NUM;
    mhandle_dup(&(show_msg->mh), mh);
    
    show_msg->msg.dbmgr_ha_total_count_rsp = rsp;
    
    mhandle_get_msg(DbmgrTotalCountRequest, req, mh);
    ret = db_open(DBTBLID(HA, SA, req->instance_num), 
                  &(show_msg->db_handle), 
                  show_msg,
                  dbmgr_ha_sa_ctrl_callback,     //attation please: for the same db_id, the callbak function should be same.
                  dbmgr_ha_sa_write_callback,
                  dbmgr_ha_sa_read_callback,
                  dbmgr_ha_sa_read_complete_callback);
    if((ret != DB_SUCCESS) && (ret != DB_PENDING)) 
    {
        db_close(show_msg->db_handle);
        show_msg->dbmgr_show_state = LTEGW_DBMGR_FREE;
    }
    else
    {
        ret = db_get_total_row_number(show_msg->db_handle, dbmgr_get_ha_total_count_callback,  show_msg);
        if((ret != DB_SUCCESS) && (ret != DB_PENDING)) 
        {
            db_close(show_msg->db_handle);
            show_msg->dbmgr_show_state = LTEGW_DBMGR_FREE;
        } 
    }
    return; 
}

static void test_request_handler(MSGHandle *mh, void *arg)
{
    static int counter;
    char line[64];
    TestMsgapiResponse rsp = MSG_INIT(TEST, MSGAPI_RESPONSE);
    mhandle_get_msg(TestMsgapiRequest, req, mh);

    printf(" sync = %d, instance = %d, filter = %s \n", req->sync, req->instance, req->filter);

    rsp.counter = counter++;
    sprintf(line, "line-%d\n", counter++);
    rsp.line = line;
    msgapi_send_reply(mh, MSGID(TEST, MSGAPI_RESPONSE), &rsp);

    return; 
}


int vnf_dbmgr_main(int argc, char *argv[])
{
    DBMgr *mgr = NULL; 
    int rc = 0;

    //there's no dbmgr specific argv[] for now

    printf("starting dbmgr task ...\n");
    scheduler_init();
    ltegw_dbmgr_msg_handle_t dbmgr_show_msg;
    mgr = dbmgr_new(&dbmgr_show_msg);

    rc = run_scheduler();

    //clean up
    dbmgr_free(mgr);
    scheduler_deinit();
    printf("dbmgr task stopped(rc = %d)\n", rc);
    return rc;
}

static DBMgr *dbmgr_new(ltegw_dbmgr_msg_handle_t * dbmgr_show_msg)
{
    DBMgr *mgr = g_malloc0(sizeof(DBMgr));
    //dbmgr is singleton
    mgr->tid = TASKID(TASK_DBMGR, 0);
    mgr->msg_api = msgapi_init(mgr->tid);

    //register all request handlers with msg_api
    msgapi_register_request_handler(mgr->msg_api, MSGID(TEST, MSGAPI_REQUEST), test_request_handler, mgr);
    msgapi_register_request_handler(mgr->msg_api, MSGID(READCDB, VNF_HA_REQUEST), dbmgr_readcdb_vnf_ha_request_handler, mgr);
    msgapi_register_request_handler(mgr->msg_api, MSGID(DBMGR,  TOTAL_COUNT_REQUEST), ha_get_count_request_handler, dbmgr_show_msg);
 
    return mgr;
}

static void dbmgr_free(DBMgr *mgr)
{
    //TODO gracefully shut down the task
    msgapi_deinit();
    g_free(mgr);
    return;
}

static void dbmgr_readcdb_vnf_ha_request_handler(MSGHandle *mh, void *arg)
{
#ifdef HAVE_VNFHA
    ReadcdbVnfHaResponse rsp = MSG_INIT(READCDB, VNF_HA_RESPONSE);
    mhandle_get_msg(ReadcdbVnfHaRequest, req, mh);

    db_handle_sync_t *db_handle = NULL;
    db_handle_sync_param_t param;
    memset(&param, 0, sizeof(param));
    int ret = db_open_sync(DBTBLID(CDB, VNF_HA, DBTBL_RESERVED_CDB_INST), &param, &db_handle);
    if((ret != DB_SUCCESS) && (ret != DB_PENDING)) {
        db_handle = NULL;
        printf("%s db_open_sync failed with error %d\n", __FUNCTION__, ret);
        rsp.rc = ret;
    }
    if(!db_handle) {
        printf("%s db_open_sync failed to return db_handle\n", __FUNCTION__);
        rsp.rc = -1;
    }
    else {
        db_column_t db_columns[1];
        db_columns[0].column_id    = DBTBLCOLID(CDB, VNF_HA, VALUE);
        db_columns[0].record_type  = RECORDID(CDB, VNF_HA);
        db_columns[0].record = NULL;
        
        ret = db_read_single_row_columns_sync(db_handle,
                                              (uint8_t*)vnf_id_g, sizeof(vnf_id_g),
                                              db_columns, 1);
        if((ret != DB_SUCCESS) && (ret != DB_PENDING)) {
            printf("%s db_read_single_columns_sync returns error %d", __FUNCTION__, ret);
            rsp.rc = -1;
        }
        else {
            CdbVnfHa * cdbVnfHa = (CdbVnfHa*) db_columns[0].record;
            if(cdbVnfHa) {
                rsp.has_preferred_role    = cdbVnfHa->has_preferred_role;
                rsp.preferred_role        = cdbVnfHa->preferred_role;
                rsp.self_interface        = cdbVnfHa->self_interface;
                rsp.self_interface_ip_ver = cdbVnfHa->self_interface_ip_ver;
                rsp.has_remote_ip         = cdbVnfHa->has_remote_ip;
                rsp.remote_ip.len         = cdbVnfHa->remote_ip.len;
                rsp.remote_ip.data        = cdbVnfHa->remote_ip.data;
                rsp.has_remote_port       = cdbVnfHa->has_remote_port;
                rsp.remote_port           = cdbVnfHa->remote_port;
                rsp.has_local_mgmt_ip     = cdbVnfHa->has_local_mgmt_ip;
                rsp.local_mgmt_ip.len     = cdbVnfHa->local_mgmt_ip.len;
                rsp.local_mgmt_ip.data    = cdbVnfHa->local_mgmt_ip.data;
                rsp.has_remote_mgmt_ip    = cdbVnfHa->has_remote_mgmt_ip;
                rsp.remote_mgmt_ip.len    = cdbVnfHa->remote_mgmt_ip.len;
                rsp.remote_mgmt_ip.data   = cdbVnfHa->remote_mgmt_ip.data;
                rsp.has_remote_heartbeat_enable  = cdbVnfHa->has_remote_heartbeat_enable;
                rsp.remote_heartbeat_enable      = cdbVnfHa->remote_heartbeat_enable;
                rsp.has_active_node_selection    = cdbVnfHa->has_active_node_selection;
                rsp.active_node_selection        = cdbVnfHa->active_node_selection;
                rsp.has_remote_heartbeat_timeout = cdbVnfHa->has_remote_heartbeat_timeout;
                rsp.remote_heartbeat_timeout     = cdbVnfHa->remote_heartbeat_timeout;
                rsp.has_task_down_max            = cdbVnfHa->has_task_down_max;
                rsp.task_down_max                = cdbVnfHa->task_down_max;
                rsp.has_local_poll_timeout       = cdbVnfHa->has_local_poll_timeout;
                rsp.local_poll_timeout           = cdbVnfHa->local_poll_timeout;
                rsp.has_remote_mgmt_intf_reply_timeout  = cdbVnfHa->has_remote_mgmt_intf_reply_timeout;
                rsp.remote_mgmt_intf_reply_timeout       = cdbVnfHa->remote_mgmt_intf_reply_timeout;
            
                rsp.rc = 0;
            }
            else {
                rsp.rc = -1;
            }
        }

        msgapi_send_reply(mh, MSGID(READCDB, VNF_HA_RESPONSE), &rsp);

        //free memory from db api after we finishing using them in reply message
        ret = db_free_read_sync(db_handle,(uint8_t*)vnf_id_g, sizeof(vnf_id_g), db_columns, 1);
        if((ret != DB_SUCCESS) && (ret != DB_PENDING)) {
            printf("%s db_free_read_sync returns error %d", __FUNCTION__, ret);
        }
        ret = db_close_sync(db_handle);
        if((ret != DB_SUCCESS) && (ret != DB_PENDING)) {
            printf("%s db_close_sync returns error %d", __FUNCTION__, ret);
        }
    }

    return; 
#endif
}
