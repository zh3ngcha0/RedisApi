#include <dbmgr.h>
#include "dbmgr_impl.h"
#include <scheduler.h>


#include <msg_test.h>
#include <msg_test.pb-c.h>
#include <msg_dbmgr.h>
#include <msg_dbmgr.pb-c.h>

#include <glib.h>
#include <stdio.h>
#include "db_api.h"
#include "dbmgr_info_get.h"


void dbmgr_ha_sa_ctrl_callback(void *cb_arg, db_error_t err)
{
    #ifdef CASA_NFV_HA_TEST
    printf("%s err %d, cb_arg %p\r\n", 
           __FUNCTION__, err, cb_arg);
    #endif
    if(!cb_arg) 
    {
        return;
    }
    if((err != DB_SUCCESS) && (err != DB_PENDING)) 
    {
        ltegw_dbmgr_msg_handle_t * ha_sa_msg  = (ltegw_dbmgr_msg_handle_t *)cb_arg;
        db_close(ha_sa_msg->db_handle);
        ha_sa_msg->dbmgr_show_state = LTEGW_DBMGR_FREE;
    }
}

void dbmgr_ha_sa_write_callback(db_error_t err,
                                 void *cb_arg, 
                                 uint8_t * key, uint8_t key_len)
{
    #ifdef CASA_NFV_HA_TEST
    printf("%s err %d, cb_arg %p, key 0x%x, key_len %d\r\n", 
           __FUNCTION__, err, cb_arg, key, key_len);
    #endif
    if(!cb_arg)
    {
        return;
    }
    ltegw_dbmgr_msg_handle_t * ha_sa_msg  = (ltegw_dbmgr_msg_handle_t *)cb_arg;
    if((err != DB_SUCCESS) && (err != DB_PENDING)) 
    {
        db_close(ha_sa_msg->db_handle);    
        ha_sa_msg->dbmgr_show_state = LTEGW_DBMGR_FREE;    
    }
}

void dbmgr_ha_sa_read_callback(db_error_t err,
                                void *cb_arg,
                                uint8_t * key, uint8_t key_len, 
                                db_column_t *reply_data, size_t reply_data_num)
{
#ifdef CASA_NFV_HA_TEST
    printf("%s err %d, cb_arg %p\r\n", 
           __FUNCTION__, err, cb_arg);
#endif
    if(!cb_arg) 
    {
        return;
    }
    if((err != DB_SUCCESS) && (err != DB_PENDING)) 
    {
        ltegw_dbmgr_msg_handle_t * ha_sa_msg  = (ltegw_dbmgr_msg_handle_t *)cb_arg;
        db_close(ha_sa_msg->db_handle);
        ha_sa_msg->dbmgr_show_state = LTEGW_DBMGR_FREE;
        return;
    }
}

void dbmgr_ha_sa_read_complete_callback(db_error_t err,
                                         void *cb_arg) //application defined
{
    #ifdef CASA_NFV_HA_TEST
    printf("%s err %d, cb_arg %p\r\n",  __FUNCTION__, err, cb_arg);
    #endif
   if(!cb_arg) {
        return;
    } 
    ltegw_dbmgr_msg_handle_t * ha_sa_msg  = (ltegw_dbmgr_msg_handle_t *)cb_arg;
    ha_sa_msg->dbmgr_show_state = LTEGW_DBMGR_FREE;  
    db_close(ha_sa_msg->db_handle);
}

void dbmgr_get_ha_total_count_callback (db_error_t err, void *cb_arg, int answer)
{
    if (cb_arg == NULL)
    {
        return;
    }
    ltegw_dbmgr_msg_handle_t * ha_sa_msg  = (ltegw_dbmgr_msg_handle_t *)cb_arg;
    if((err != DB_SUCCESS) && (err != DB_PENDING)) 
    {
        db_close(ha_sa_msg->db_handle);
        ha_sa_msg->dbmgr_show_state = LTEGW_DBMGR_FREE;
        return;
    }
    ha_sa_msg->msg.dbmgr_ha_total_count_rsp.total_count = answer;
    msgapi_send_reply(&(ha_sa_msg->mh), MSGID(DBMGR,  TOTAL_COUNT_RESPONSE), & (ha_sa_msg->msg.dbmgr_ha_total_count_rsp));
    db_close((ha_sa_msg->db_handle));
    ha_sa_msg->dbmgr_show_state = LTEGW_DBMGR_FREE;
}

