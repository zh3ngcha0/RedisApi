#include <stdio.h>

#include "db_api.h"
#include "db_impl.h"
#include <glib.h>
#include "db_ha.h"
#include <string.h>

#include <stdlib.h>
#include <hiredis/async.h>

#include "scheduler.h"
#include <hiredis/adapters/libevent.h>

#include "record_table.h"
#include <hiredis/hiredis.h>
#include <stdint.h>
#include "record_ha.h"
#include "record_ha.pb-c.h"

#include "db_api_log.h"
#include "db_api_util.h"
#include "db_api_common.h"

#define  DB_ERROR_DEF(X, Y) Y,
char * db_error_strings[] = { DB_ERROR_LIST(DB_ERROR_DEF) };
#undef DB_ERROR_DEF

#undef DBIDTONAME
#define DBIDTONAME(id) #id
#undef XX 

const char *DBTBLGroup_name[] = {
    DBTBL_GROUP_HA,
    DBTBL_GROUP_CDB,
    DBTBL_GROUP_ALARM,
    DBTBL_GROUP_MAXNUM,
    "unknown name" //last line
};

const char *DBHaTBL_name[] = {
    DBTBL_HA_IPSEC_START_TIME, 
    DBTBL_HA_SA,               
    DBTBL_HA_IPPOOL,           
    DBTBL_HA_SWM_DIAM,         
    DBTBL_HA_EPDG_SESSION,     
    DBTBL_HA_EPDG_BEARER,      
    DBTBL_HA_S1MME_INST_CFG,   
    DBTBL_HA_S1MME_LENB,       
    DBTBL_HA_S1MME_MME,        
    DBTBL_HA_S1MME_SUPTD_TAI,  
    DBTBL_HA_S1MME_HENB_CB,    
    DBTBL_HA_S1MME_X2_HENB,             
    DBTBL_HA_S1MME_X2_ENB,              
    DBTBL_HA_S1MME_X2_SERVED_CELL,      
    DBTBL_HA_HENBM_INST_CFG,    
    DBTBL_HA_HENB_DATA,         
    DBTBL_HA_UE_DATA,           
    DBTBL_HA_TE_DATA,           
    DBTBL_HA_S1MME_SCTP_RED_GLB,        
    DBTBL_HA_S1MME_SCTP_RED_EP,         
    DBTBL_HA_X2_HENB_DATA,      
    DBTBL_HA_TE_GTPU,                  
    DBTBL_HA_HENB_SCTP_RED_GLB,        
    DBTBL_HA_HENB_SCTP_RED_EP,         
    DBTBL_HA_LAST_ONE,
    "unknown name" //last line
};

const char *DBHaIpsecStartTimeColumn_name[] = {
#define XX(tag) DBIDTONAME(HA_IPSEC_START_TIME_##tag),
    DBHaIpsecStartTimeColumn_MAP(XX)
#undef XX
    "unknown name" //last line
};

const char *DBHaSaColumn_name[] = {
    DBTBL_HA_SA_IKE_PEER,               
    DBTBL_HA_SA_IKE_INFO,                
    DBTBL_HA_SA_IKE_STAT,                
    DBTBL_HA_SA_CHILD_INFO_IB,           
    DBTBL_HA_SA_CHILD_INFO_OB,           
    DBTBL_HA_SA_CHILD_STAT_IB,           
    DBTBL_HA_SA_CHILD_STAT_OB,           
    DBTBL_HA_SA_LAST_ONE, 
    "unknown name" //last line
};


//#define DEBUG_DB
int logging_level_db_api = L_ERRORS;

const char *db_local_ip_default = "127.0.0.1";
const int   db_local_port_default = 6379;

static db_handle_t *db_handle_notify_control;
static db_handle_t *db_handle_notify_data;
static GQueue *notify_entry_queue;

GQueue *db_handle_queue;
GQueue *db_handle_queue_sync;
RecordMetaInfo *HaChRowMsgFns;
int local_is_master = 1; 
char remote_server_ip[48];

static void db_init_internal(void);
static void issue_read_commands(db_handle_t *db_handle, redisReply *reply);
static void db_cmd_callback_fn(redisAsyncContext *ctx, void *reply_returned, void *privdata);
static db_error_t db_exec_cmd(db_handle_t *db_handle, 
                              char *cmd_str1, 
                              char *cmd_str2, 
                              uint8_t *key, size_t key_len,
                              int column_id,
                              char *column_data, size_t column_data_len);
static void db_init_connect(db_handle_t *db_handle);
static void db_cleanup_state(db_handle_t *db_handle);
static void issue_del_row_commands(db_handle_t *db_handle, redisReply *reply);

#define IS_DB_RET_SUCCESS(ret) (ret == DB_SUCCESS || ret == DB_PENDING)


const char *db_error_str(db_error_t err)
{
    VNF_ASSERT(err < DB_ERROR_INVALID);

    return db_error_strings[err];
}

static void db_close_ctx(db_handle_t *db_handle)
{
    if (db_handle != NULL)
    {
        if (db_handle->redis_ctx != NULL) 
        {
            if (!db_handle->redis_ctx->err) 
            {
                /* special cmd, no need to use db_exec_cmd() */
                redisAsyncCommand(db_handle->redis_ctx, NULL, (void*)(db_handle), "quit");
            }

            redisAsyncFree(db_handle->redis_ctx);

            db_handle->redis_ctx = NULL;
        }
    }
}

static void db_flush_queued_cmd(gpointer cmd, gpointer user_data)
{
    db_handle_t *db_handle = (db_handle_t*)user_data;
    db_cmd_data_t *temp = (db_cmd_data_t*)cmd;
    VNF_ASSERT(db_handle);
    int ret = REDIS_OK;
    
    //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "using redis_ctx", db_handle->redis_ctx);

    /* re-execute */
    if (temp->cmd_type == DB_CMD_STR1_ONLY)
    {
        ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, (void*)(db_handle), "%s", temp->cmd);
    } 
    else if (temp->cmd_type == DB_CMD_STR1_STR2)
    {
        ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, (void*)(db_handle), "%s %s", temp->cmd, temp->cmd2);
    } 
    else if (temp->cmd_type == DB_CMD_WITH_KEY)
    {
        if (temp->cmd2[0] != '\0')
            ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, (void*)(db_handle), "%s %s %b", temp->cmd, temp->cmd2, temp->key, temp->key_len);
        else
        {
            if (strncasecmp(temp->cmd, REDIS_BATCH_END_CMD, strlen(REDIS_BATCH_END_CMD)) == 0)
            {
                ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, db_handle, "%s", temp->cmd);    
            }
            else
                ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, (void*)(db_handle), "%s %b", temp->cmd, temp->key, temp->key_len);
        }
    } 
    else if (temp->cmd_type == DB_CMD_WITH_DATA)
    {
        if (temp->cmd2[0] != '\0')
            ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, (void*)(db_handle), "%s %s %b %b %b", temp->cmd, temp->cmd2, temp->key, temp->key_len,
                    (char *) &temp->column_id, sizeof(temp->column_id), temp->column_data, temp->column_data_len);
        else
        {
            ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, (void*)(db_handle), "%s %b %b %b", temp->cmd, temp->key, temp->key_len, 
                    (char *) &temp->column_id, sizeof(temp->column_id), temp->column_data, temp->column_data_len);
        }
    }
    else
    {
        VNF_ASSERT(0); // impossible
    }

    //DB_API_LOG(DB_API_EVENT_REPEAT_CMD, __FUNCTION__, __LINE__, db_cmd_to_str(temp), ret);
}

static void db_cmd_failure(db_handle_t *db_handle, db_cmd_data_t *cmd_data)
{
    if (db_handle->full_read_in_progress && 
        strncasecmp(cmd_data->cmd, REDIS_GET_ALL_IDS_CMD, strlen(REDIS_GET_ALL_IDS_CMD)) == 0) {
        //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "get all ids failed", 0);
        db_handle->full_read_in_progress = 0;
        db_handle->read_complete_cb(DB_CONN_FAILED, db_handle->cb_data);
    }

    if (strncasecmp(cmd_data->cmd, REDIS_GET_SINGLE_ROW_CMD, strlen(REDIS_GET_SINGLE_ROW_CMD)) == 0) {
        //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "get single row cmd failed", 0);
        db_column_t columns[1];
        int column_num = 0;
        db_column_t *column = &columns[0];
        column_num = 0;
 
        /* invoke read call back */
        column = &columns[0];
        db_handle->read_cb(DB_CONN_FAILED, db_handle->cb_data, 
                           cmd_data->key, cmd_data->key_len,
                           column, column_num); 
        
        db_handle->read_state.number_of_rows --;
        if (db_handle->read_state.number_of_rows <= 0) {
            db_handle->read_complete_cb(DB_CONN_FAILED, db_handle->cb_data);
        }

    }
    else if (strncasecmp(cmd_data->cmd, REDIS_BATCH_START_CMD, strlen(REDIS_BATCH_START_CMD)) == 0) {
        //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "batch start cmd failed", 0);
        db_handle->batch_cb_start = 1;
    }            
    else if (strncasecmp(cmd_data->cmd, REDIS_WRITE_CMD, strlen(REDIS_WRITE_CMD)) == 0) {
        //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "write cmd failed", 0);
        db_handle->write_cmd_cb_received = 1;
    }
    else if (strncasecmp(cmd_data->cmd, REDIS_BATCH_END_CMD, strlen(REDIS_BATCH_END_CMD)) == 0) {
        if (db_handle->write_cmd_cb_received) {
           //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "batch end cmd failed", 0);
            
            db_handle->write_cb(DB_CONN_FAILED, db_handle->cb_data, 
                                cmd_data->key, cmd_data->key_len);
            db_handle->write_cmd_cb_received = 0;
        }
        db_handle->batch_cb_start = 0;
    }
}

static void db_empty_cmd_queue(db_handle_t *db_handle)
{
    VNF_ASSERT(db_handle);
    VNF_ASSERT(db_handle->pending_cmd_queue);
    
    db_cmd_data_t * cmd_data = NULL;

    do 
    {
        cmd_data = (db_cmd_data_t*)g_queue_pop_head(db_handle->pending_cmd_queue);
        if (cmd_data != NULL)
        {
            //pass error to application for the queued requests
            db_cmd_failure(db_handle, cmd_data);
            g_slice_free(db_cmd_data_t, cmd_data);
        }
    } while (cmd_data != NULL);

}

static void db_disconnect_fn(const struct redisAsyncContext *ctx, int status)
{
    db_handle_t * db_handle = (db_handle_t*) ctx->data;

    //DB_API_LOG(DB_API_EVENT_DB_ERROR, __FUNCTION__, __LINE__, ctx->errstr);

    if (db_handle != NULL)
    {
        if(db_handle->ctrl_cb) {
           db_handle->ctrl_cb(db_handle->cb_data, DB_CONN_FAILED);
        }
        db_handle->conn_state = DB_DISCONNECTED;
        db_empty_cmd_queue(db_handle);
        db_cleanup_state(db_handle);
        db_handle->redis_ctx = NULL;
        // db_close_ctx(db_handle);
    }

}

static void db_connect_fn(const struct redisAsyncContext *ctx, int status)
{
    db_handle_t * db_handle = (db_handle_t*) ctx->data;

    //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG,  __FUNCTION__, __LINE__, "db connetion state", db_handle->conn_state);
    //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG,  __FUNCTION__, __LINE__, "status", status);
    //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG,  __FUNCTION__, __LINE__, "db_handle", db_handle);

    if (db_handle != NULL)
    {
        if (status != REDIS_OK)
        {
            db_handle->ctrl_cb(db_handle->cb_data, DB_CONN_FAILED);
            db_handle->conn_state = DB_DISCONNECTED;
            
            db_init_connect(db_handle);
        }
        else
        {
            db_handle->conn_state = DB_CONNECTED;
            VNF_ASSERT(db_handle->redis_ctx == ctx);

            //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "run through saved cmds", 0);
            
            g_queue_foreach(db_handle->pending_cmd_queue, db_flush_queued_cmd, db_handle);
        }
    }
}

static void db_cleanup_state(db_handle_t *db_handle)
{
    db_handle->full_read_in_progress = FALSE;
    db_handle->delete_all_rows_in_progress = FALSE;
    
    // no callback here
}

/* calls this function when encountering an error in DB operation */
static void db_state_handle_error(db_handle_t *db_handle)
{
#ifdef DEBUG_DB
    printf("in %s\n", __FUNCTION__);
#endif    
    if (db_handle->conn_state == DB_INIT || 
        db_handle->conn_state == DB_CONNECTING)
    {
        // do nothing
    } 
    else if (db_handle->conn_state == DB_CONNECTED || 
             db_handle->conn_state == DB_DISCONNECTED) 
    {
        db_cleanup_state(db_handle);

        db_init_connect(db_handle);
    } 
}

static void db_init_connect(db_handle_t *db_handle)
{  
#ifdef DEBUG_DB
    printf("in %s\n", __FUNCTION__);
#endif

    //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "db connection state", db_handle->conn_state);

    /* close previous ctx if there is */
    if(db_handle->conn_state != DB_DISCONNECTED) {
        db_close_ctx(db_handle);
    }
    
    /* open connection to Redis */
    redisAsyncContext *ctx = redisAsyncConnect(db_local_ip_default, db_local_port_default);
    //DB_API_LOG(DB_API_EVENT_REDIS_CTX, __FUNCTION__, __LINE__, "create new redis ctx", ctx);

    VNF_ASSERT(ctx != NULL);
    VNF_ASSERT(ctx->err == 0);
    
    db_handle->redis_ctx = ctx;    
    db_handle->conn_state = DB_CONNECTING;

    ctx->data = db_handle;

    redisAsyncSetConnectCallback(ctx, db_connect_fn);
    redisAsyncSetDisconnectCallback(ctx, db_disconnect_fn);

    redisAsyncCommand(db_handle->redis_ctx, NULL, (void*)(db_handle), "select 0");
    
    redisLibeventAttach(ctx, get_scheduler());

}

static int bin_to_str(char* buf, int buf_len, char* out_buf, int out_buf_len)
{
    int i = 0, index = 0;

    snprintf(out_buf, out_buf_len, "0x");
    index += 2;

    for (i=0; i<buf_len; i++)
    {
        snprintf(out_buf+index, out_buf_len-index, "%02x", (uint8_t)buf[i]);
        index += 2;
    }

    return index;
}

char * reply_to_str(redisReply *reply, char* reply_str, int reply_str_max_len, int *output_len)
{
    int len = 0, temp_len = 0; 
    *output_len = 0;

    int i=0;

    len += sprintf(reply_str, "[ ");
    
    if(reply == NULL) {
        snprintf(reply_str+len, reply_str_max_len-len-1, "NULL ]");
        reply_str[len] = '\0';
        return reply_str;
    }
    switch (reply->type) {
    case REDIS_REPLY_STRING:
        len += bin_to_str(reply->str, reply->len, reply_str+len, reply_str_max_len-len);
        break;
    case REDIS_REPLY_ARRAY:
        len += snprintf(reply_str+len, reply_str_max_len-len-1, "Array : %ld elements", reply->elements);

        for (i=0; i<reply->elements; i++)
            {
                if (len < (reply_str_max_len-128))
                {
                    reply_to_str(reply->element[i], reply_str+len, reply_str_max_len-len-1, &temp_len);
                    len += temp_len;
                }
            }
        break;
    case REDIS_REPLY_INTEGER:
        len += snprintf(reply_str+len, reply_str_max_len-len-1, "Int : %lld", reply->integer);
        break;
    case REDIS_REPLY_NIL:
        len += snprintf(reply_str+len, reply_str_max_len-len-1, "NULL");
        break;
    case REDIS_REPLY_STATUS:
        len += snprintf(reply_str+len, reply_str_max_len-len-1, "Status: %lld", reply->integer);
        break;
    case REDIS_REPLY_ERROR:
        len += snprintf(reply_str+len, reply_str_max_len-len-1, "Error: %s", reply->str);
        break;        
        
    default:
        len += snprintf(reply_str+len, reply_str_max_len-len-1, "Unknown type %d", reply->type);
        break;
    }

    len += sprintf(reply_str+len, "]");

    reply_str[len] = '\0';

    *output_len = len;

    return reply_str;
}

static void db_cmd_callback_fn(redisAsyncContext *ctx, void *reply_returned, void *privdata)
{
    db_handle_t *db_handle = (db_handle_t*) ctx->data;
    //char *cmd = (char*)privdata;
    char reply_str[DB_REPLY_STR_MAX_LEN];
    redisReply * reply = (redisReply*)reply_returned;
    int temp_len = 0;
    
    if (db_handle != NULL && db_handle->magic == DB_HANDLE_MAGIC)
    {
        db_cmd_data_t *cmd_data = (db_cmd_data_t*)g_queue_pop_head(db_handle->pending_cmd_queue);

        if (cmd_data != NULL)
        {

            //DB_API_LOG(DB_API_EVENT_RECV_CMD_CB, __FUNCTION__, __LINE__, db_cmd_to_str(cmd_data));

#ifdef DEBUG_DB
            printf("in %s cmd is %s\n", __FUNCTION__, db_cmd_to_str(cmd_data));
#endif
        }

#ifdef DEBUG_DB
        printf("in %s full_read_in_progress=%d, reply is %s\n", __FUNCTION__, db_handle->full_read_in_progress, reply_to_str(reply, reply_str, DB_REPLY_STR_MAX_LEN, &temp_len));
#endif

        if (reply != NULL && reply->type != REDIS_REPLY_ERROR)
        {
            //DB_API_LOG(DB_API_EVENT_REPLY_IS, __FUNCTION__, __LINE__, reply_to_str(reply, reply_str, DB_REPLY_STR_MAX_LEN, &temp_len),  db_handle->full_read_in_progress, reply->elements);

            if (db_handle->full_read_in_progress && strncasecmp(cmd_data->cmd, REDIS_GET_ALL_IDS_CMD, strlen(REDIS_GET_ALL_IDS_CMD)) == 0)
            {
                /* now need to parse reply and issue read for each row */
                redisReply * temp = reply;

                if (temp && temp->type == REDIS_REPLY_ARRAY && temp->elements > 0)
                {
                    issue_read_commands(db_handle, temp);
                    db_handle->full_read_in_progress = 0;
                }
                else {
                    db_handle->full_read_in_progress = 0;
                    db_handle->read_complete_cb(DB_SUCCESS, db_handle->cb_data);
                }
            }

            if (db_handle->delete_all_rows_in_progress && strncasecmp(cmd_data->cmd, REDIS_GET_ALL_IDS_CMD, strlen(REDIS_GET_ALL_IDS_CMD)) == 0)
            {
                /* now need to parse reply and issue read for each row */
                redisReply * temp = reply;

                if (temp && temp->type == REDIS_REPLY_ARRAY && temp->elements > 0)
                {
                    issue_del_row_commands(db_handle, temp);
                }
                else {
                    // empty table ?
                    db_handle->delete_all_rows_in_progress = 0;
                }
            }

            if (db_handle->delete_all_rows_in_progress && strncasecmp(cmd_data->cmd, REDIS_DEL_CMD, strlen(REDIS_DEL_CMD)) == 0) {
                db_handle->delete_all_rows_in_progress = 0;
            }

            if (strncasecmp(cmd_data->cmd, REDIS_GET_SINGLE_ROW_CMD, strlen(REDIS_GET_SINGLE_ROW_CMD)) == 0)
            {
                //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "get single row cmd callback", 0);
                db_column_t columns[DB_MAX_COLUMNS];
                int column_num = 0, i = 0;
                db_column_t *column = &columns[0];
                VNF_ASSERT(reply->elements%2 == 0);
                column_num = reply->elements/2;
                
                for (i = 0; i<reply->elements; i=i+2)
                {
                    memcpy(&column->column_id, reply->element[i]->str, reply->element[i]->len);
               
                    redisReply *oneReply = reply->element[i+1];
                    
                    VNF_ASSERT(oneReply->type == REDIS_REPLY_STRING);
                    VNF_ASSERT(oneReply->len >= sizeof(int));

                    column->record_type = *((int*)(oneReply->str));
                    RecordMetaInfo * rm = lookup_record_meta_table(column->record_type);
                    VNF_ASSERT(rm != NULL);

                    column->record = (ProtobufCMessage*)(rm->unpack(NULL, oneReply->len-4, oneReply->str+4));

                    VNF_ASSERT(column->record != NULL);

                    column++;
                }

#ifdef DEBUG_DB
                printf("about to call read_cb\n");
#endif 
                /* invoke read call back */
                column = &columns[0];
                db_handle->read_cb(DB_SUCCESS, db_handle->cb_data, 
                                   cmd_data->key+DB_CMD_HASHKEY_TAG_LEN, cmd_data->key_len-DB_CMD_HASHKEY_TAG_LEN,
                                   column, column_num); 
                
                for (i = 0; i<column_num; i++)
                {
                    RecordMetaInfo * rm = lookup_record_meta_table(column->record_type);
                    rm->free_unpacked(column->record, NULL);
                    column++;
                }

                db_handle->read_state.number_of_rows --;
                if (db_handle->read_state.number_of_rows <= 0)
                {
                    db_handle->read_complete_cb(DB_SUCCESS, db_handle->cb_data);
                }

            }
            else if (strncasecmp(cmd_data->cmd, REDIS_BATCH_START_CMD, strlen(REDIS_BATCH_START_CMD)) == 0)
            {
                db_handle->batch_cb_start = 1;
            }            
            else if (strncasecmp(cmd_data->cmd, REDIS_WRITE_CMD, strlen(REDIS_WRITE_CMD)) == 0)
            {
                db_handle->write_cmd_cb_received = 1;
            }
            else if (strncasecmp(cmd_data->cmd, REDIS_BATCH_END_CMD, strlen(REDIS_BATCH_END_CMD)) == 0)
            {
                if (db_handle->write_cmd_cb_received)
                {

                    //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "about to call write_cb", 0);

                    db_handle->write_cb(DB_SUCCESS, db_handle->cb_data, 
                                        cmd_data->key+DB_CMD_HASHKEY_TAG_LEN, cmd_data->key_len-DB_CMD_HASHKEY_TAG_LEN);
                    db_handle->write_cmd_cb_received = 0;
                }
                db_handle->batch_cb_start = 0;
            }
        }
        else {
            if (ctx->err) {
                //DB_API_LOG(DB_API_EVENT_DB_ERROR, __FUNCTION__, __LINE__, ctx->errstr);
            }
            else if (reply == NULL) {
                //DB_API_LOG(DB_API_EVENT_DB_ERROR, __FUNCTION__, __LINE__, "reply is NULL and ctx has no error\n");
            } 
            else if (reply->type == REDIS_REPLY_ERROR) {
                //DB_API_LOG(DB_API_EVENT_REPLY_ERROR, __FUNCTION__, __LINE__, reply_to_str(reply, reply_str, DB_REPLY_STR_MAX_LEN, &temp_len), 
                //           db_handle->full_read_in_progress, reply->elements);
            }
             
            //pass error to application for the queued requests
            db_cmd_failure(db_handle, cmd_data);
        }

    
        if (cmd_data != NULL)
        {
            g_slice_free(db_cmd_data_t, cmd_data);
        }

        //db_handle->ctrl_cb(db_handle->cb_data, DB_SUCCESS);
    }    
    else{

#ifdef DEBUG_DB
        printf("%s, db_handle is illegal !!! %p\n",
                __FUNCTION__, db_handle);
#endif         
    }
    return;
}

static db_error_t db_exec_cmd(db_handle_t *db_handle, 
                              char *cmd_str1, 
                              char *cmd_str2,
                              uint8_t *key, size_t key_len,
                              int column_id,
                              char *column_data, size_t column_data_len)
{
    VNF_ASSERT(cmd_str1);
    VNF_ASSERT(db_handle->pending_cmd_queue);
    
    int ret = REDIS_OK;

    db_cmd_data_t *temp = g_slice_new0(db_cmd_data_t);
    
    strcpy(temp->cmd, cmd_str1);

    if (key == NULL)
    {
        temp->key_len = 0;

        if (cmd_str2 == NULL)
        {
            temp->cmd_type = DB_CMD_STR1_ONLY;
            
            if (db_handle->conn_state == DB_CONNECTED)
            {
                ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, db_handle, "%s", temp->cmd);    
            }
        }
        else
        {
            strcpy(temp->cmd2, cmd_str2);
            temp->cmd_type = DB_CMD_STR1_STR2;
            
            if (db_handle->conn_state == DB_CONNECTED)
            {
                ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, db_handle, "%s %s", temp->cmd, temp->cmd2);    
            }

        }
    } 
    else if (column_data == NULL)
    {
        temp->cmd_type = DB_CMD_WITH_KEY;
        
        if (cmd_str2 != NULL)
            strcpy(temp->cmd2, cmd_str2);
        else
            temp->cmd2[0] = '\0';

        memcpy(temp->key, key, key_len);
        temp->key_len = key_len;
        temp->column_id = column_id;
        temp->column_data[0] = '\0';
        temp->column_data_len = 0;

        if (db_handle->conn_state == DB_CONNECTED)
        {
            if (temp->cmd2[0] != '\0')
                ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, db_handle, "%s %s %b", temp->cmd, temp->cmd2, key, key_len);    
            else
            {
                if (strncasecmp(cmd_str1, REDIS_BATCH_END_CMD, strlen(REDIS_BATCH_END_CMD)) == 0)
                {
                    ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, db_handle, "%s", temp->cmd);    
                }
                else
                    ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, db_handle, "%s %b", temp->cmd, key, key_len);    
            }

        }
    }
    else
    {
        temp->cmd_type = DB_CMD_WITH_DATA;

        if (cmd_str2 != NULL)
            strcpy(temp->cmd2, cmd_str2);
        else
            temp->cmd2[0] = '\0';

        memcpy(temp->key, key, key_len);
        temp->key_len = key_len;

        temp->column_id = column_id;
        VNF_ASSERT(column_data_len < DB_CMD_DATA_MAX_LEN);
        memcpy(temp->column_data, column_data, column_data_len);
        temp->column_data_len = column_data_len;

        if (db_handle->conn_state == DB_CONNECTED)
        {
            if (temp->cmd2[0] != '\0')
                ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, db_handle, "%s %s %b %b %b", temp->cmd, temp->cmd2, temp->key, temp->key_len, 
                        (char *) &temp->column_id, sizeof(temp->column_id), temp->column_data, temp->column_data_len);    
            else
                ret = redisAsyncCommand(db_handle->redis_ctx, db_cmd_callback_fn, db_handle, "%s %b %b %b", temp->cmd, temp->key, temp->key_len, 
                        (char *) &temp->column_id, sizeof(temp->column_id), temp->column_data, temp->column_data_len);                        
        }
    }
    
    g_queue_push_tail(db_handle->pending_cmd_queue, temp);

    if (ret == REDIS_ERR)
    {
        if (db_handle->redis_ctx->err)
        {
            //DB_API_LOG(DB_API_EVENT_EXEC_CMD_RET, __FUNCTION__, __LINE__, db_cmd_to_str(temp), ret, db_handle->redis_ctx->errstr);
        }
        else
        {
            //DB_API_LOG(DB_API_EVENT_EXEC_CMD_RET, __FUNCTION__, __LINE__, db_cmd_to_str(temp), ret, "no ctx error");
        }
    } 
    else if (db_handle->conn_state != DB_CONNECTED) 
    {
        //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "db is not connected", 0);
        //DB_API_LOG(DB_API_EVENT_EXEC_CMD, __FUNCTION__, __LINE__, db_cmd_to_str(temp));

#ifdef DEBUG_DB
        printf("in %s db is not connected\n", __FUNCTION__);
        printf("in %s cmd is %s\n", __FUNCTION__, db_cmd_to_str(temp));
#endif

        ret = REDIS_ERR;
        return DB_PENDING;
    }
    else
    {
        //DB_API_LOG(DB_API_EVENT_EXEC_CMD, __FUNCTION__, __LINE__, db_cmd_to_str(temp));
#ifdef DEBUG_DB
        printf("in %s db is connected, cmd is %s\n", __FUNCTION__, db_cmd_to_str(temp));
#endif
    }

    if (ret != REDIS_OK)
        return DB_ERROR;

    return DB_SUCCESS;
}

/* read contents of each row. One command per row. */
static void issue_read_commands(db_handle_t *db_handle, redisReply *reply)
{
    int i = 0;
    uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
    int hashkey_len = 0;

    VNF_ASSERT(reply);
    VNF_ASSERT(reply->elements > 0);
    VNF_ASSERT(db_handle);

    db_handle->read_state.number_of_rows = 0;

    for (i=0; i<reply->elements; i++)
    {
        redisReply *rp = reply->element[i];

        if (rp && rp->len > 0 && rp->type == REDIS_REPLY_STRING)
        {
            hashkey_len = construct_hashkey_common(db_handle->id, (uint8_t*)rp->str, rp->len, hashkey, sizeof(hashkey));
            db_exec_cmd(db_handle, REDIS_GET_SINGLE_ROW_CMD, NULL, hashkey, hashkey_len, -1, NULL, 0);
           
            db_handle->read_state.number_of_rows++;

        }
    }

#ifdef DEBUG_DB
    printf("in %s number of rows is %d\n", __FUNCTION__, 
        db_handle->read_state.number_of_rows);
#endif
}

/* delete each row, hashkey */ 
void issue_del_row_commands(db_handle_t *db_handle, redisReply *reply)
{
    int i = 0;
    uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
    int hashkey_len = 0;
    uint8_t setkey[128];

    VNF_ASSERT(reply);
    VNF_ASSERT(reply->elements > 0);
    VNF_ASSERT(db_handle);


    for (i=0; i<reply->elements; i++)
    {
        redisReply *rp = reply->element[i];

        if (rp && rp->len > 0 && rp->type == REDIS_REPLY_STRING)
        {
            // construct the name of the keyset
            construct_setkey_common(db_handle->id, setkey, sizeof(setkey));
            db_exec_cmd(db_handle, REDIS_DEL_ID_CMD, setkey, (uint8_t*)rp->str, rp->len, -1, NULL, 0);

            hashkey_len = construct_hashkey_common(db_handle->id, (uint8_t*)rp->str, rp->len, hashkey, sizeof(hashkey));
            db_exec_cmd(db_handle, REDIS_DEL_CMD, NULL, hashkey, hashkey_len, -1, NULL, 0);
        }
    }

    // clear in the REDIS command completion 
    //db_handle->delete_all_rows_in_progress = 0;
}

db_error_t db_open_ex(db_id_t db_id, 
                   db_handle_t **db_handle,
                   void *cb_data,
                   db_ctrl_callback_func_t ctrl_cb,
                   db_write_callback_func_t write_cb,
                   db_read_callback_func_t read_cb,
                   db_read_complete_callback_func_t read_complete_cb,
                   db_connect_mode_t mode)
{
    VNF_ASSERT(write_cb != NULL);
    VNF_ASSERT(read_cb != NULL);
    VNF_ASSERT(ctrl_cb != NULL);
    VNF_ASSERT(read_complete_cb != NULL);

    db_init_internal();

    if (!local_is_master 
            && mode == DB_CONNECT_MASTER
            && remote_server_ip[0] == '\0')
        return DB_ERROR;

    *db_handle = g_slice_new0(db_handle_t);
    
    VNF_ASSERT(db_handle != NULL);

    /* init db handle data members */
    (*db_handle)->magic = DB_HANDLE_MAGIC;
    (*db_handle)->id = db_id;
    (*db_handle)->cb_data = cb_data;
    (*db_handle)->ctrl_cb = ctrl_cb;
    (*db_handle)->write_cb = write_cb;
    (*db_handle)->read_cb = read_cb;
    (*db_handle)->read_complete_cb = read_complete_cb;
    (*db_handle)->conn_state = DB_DISCONNECTED;
    (*db_handle)->connect_mode = mode;
    (*db_handle)->pending_cmd_queue = g_queue_new();
    (*db_handle)->reconnect_try_cnt++;
    (*db_handle)->recover_noti_queue = g_queue_new();

    g_queue_push_tail(db_handle_queue, *db_handle);

    db_init_connect(*db_handle);

    return DB_SUCCESS;
}

/* guarantee to return a db_handle that can be used for write and read, no need to reopen */
db_error_t db_open(db_id_t db_id, 
                   db_handle_t **db_handle,
                   void *cb_data,
                   db_ctrl_callback_func_t ctrl_cb,
                   db_write_callback_func_t write_cb,
                   db_read_callback_func_t read_cb,
                   db_read_complete_callback_func_t read_complete_cb)
{
    VNF_ASSERT(write_cb != NULL);
    VNF_ASSERT(read_cb != NULL);
    VNF_ASSERT(ctrl_cb != NULL);
    VNF_ASSERT(read_complete_cb != NULL);

    //Get ready for packing and unpacking db record 
    record_meta_table_init();
    *db_handle = g_slice_new0(db_handle_t);
    
    VNF_ASSERT(db_handle != NULL);

    /* init db handle data members */
    (*db_handle)->magic = DB_HANDLE_MAGIC;
    (*db_handle)->id = db_id;
    (*db_handle)->cb_data = cb_data;
    (*db_handle)->ctrl_cb = ctrl_cb;
    (*db_handle)->write_cb = write_cb;
    (*db_handle)->read_cb = read_cb;
    (*db_handle)->read_complete_cb = read_complete_cb;

    (*db_handle)->pending_cmd_queue = g_queue_new();

    db_init_connect(*db_handle);

    return DB_SUCCESS;
}



db_error_t db_close(db_handle_t *db_handle)
{
    /* close connection to Redis */
    if(db_handle == NULL) {
        return DB_HANDLE_INVALID;
    }

    db_close_ctx(db_handle);
    
    db_empty_cmd_queue(db_handle);

    g_queue_free(db_handle->pending_cmd_queue);

    db_handle->magic = 0;
    db_handle->id = 0;
    db_handle->ctrl_cb = NULL;
    db_handle->read_cb = NULL;
    db_handle->write_cb = NULL;
    db_handle->read_complete_cb = NULL;

    g_slice_free(db_handle_t, db_handle);

    db_handle = NULL;
    
    return DB_SUCCESS;
}


db_error_t db_write_single_column(db_handle_t *db_handle, 
                                  uint8_t* key, size_t key_len, 
                                  db_column_t* column)
{
    char cmd_str[DB_CMD_DATA_MAX_LEN];
    int cmd_len = 0;
    db_error_t ret = DB_SUCCESS;
    uint8_t setkey[128];
    uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
    int hashkey_len = 0;

    VNF_ASSERT(key != NULL);
    VNF_ASSERT(key_len != 0);
    VNF_ASSERT(column != NULL);

    if(db_handle == NULL) {
        return DB_HANDLE_INVALID;
    }

    ret = db_exec_cmd(db_handle, REDIS_BATCH_START_CMD, NULL, NULL, 0, -1, NULL, 0);
    if (!IS_DB_RET_SUCCESS(ret))
        goto exit;

    // construct the name of the keyset
    construct_setkey_common(db_handle->id, setkey, sizeof(setkey));
    ret = db_exec_cmd(db_handle, REDIS_ADD_ID_CMD, setkey, key, key_len, -1, NULL, 0);
    if (!IS_DB_RET_SUCCESS(ret))
        goto exit;

    cmd_len = 0;
    /* value type */
    memcpy(cmd_str+cmd_len, &column->record_type, sizeof(column->record_type));
    cmd_len += sizeof(column->record_type);

    VNF_ASSERT(cmd_len < DB_CMD_DATA_MAX_LEN);
    /* value */
    RecordMetaInfo * rm = lookup_record_meta_table(column->record_type);
    cmd_len += rm->pack(column->record, (uint8_t*)(cmd_str+cmd_len));
    VNF_ASSERT(cmd_len < DB_CMD_DATA_MAX_LEN *3/4);

    hashkey_len = construct_hashkey_common(db_handle->id, key, key_len, hashkey, sizeof(hashkey));
    ret = db_exec_cmd(db_handle, REDIS_WRITE_CMD, NULL, hashkey, hashkey_len, column->column_id, cmd_str, cmd_len);

    if (!IS_DB_RET_SUCCESS(ret))
        goto exit;

    ret = db_exec_cmd(db_handle, REDIS_BATCH_END_CMD, NULL, hashkey, hashkey_len, -1, NULL, 0);
    
    //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "cmd return is", ret);

    if (!IS_DB_RET_SUCCESS(ret))
        goto exit;

 exit:

    if (ret != DB_SUCCESS)
        db_state_handle_error(db_handle);

    return ret;
}

db_error_t db_write_single_row(db_handle_t *db_handle, 
                               uint8_t* key, size_t key_len, 
                               db_column_t *columns, size_t column_num)
{
    char cmd_str[DB_CMD_DATA_MAX_LEN];
    int cmd_len = 0, i=0;
    db_error_t ret = DB_SUCCESS;
    uint8_t setkey[128];
    uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
    int hashkey_len = 0;

    VNF_ASSERT(key != NULL);
    VNF_ASSERT(key_len != 0);
    VNF_ASSERT(columns != NULL);
    VNF_ASSERT(column_num != 0);

    if(db_handle == NULL) {
        return DB_HANDLE_INVALID;
    }

    //DB_API_LOG(DB_API_EVENT_GENERAL_DEBUG, __FUNCTION__, __LINE__, "column number is", column_num);

    ret = db_exec_cmd(db_handle, REDIS_BATCH_START_CMD, NULL, NULL, 0, -1, NULL, 0);
    if (!IS_DB_RET_SUCCESS(ret))
        goto exit;

    // construct the name of the keyset - note we are storing the app key in the set and not the hashkey
    construct_setkey_common(db_handle->id, setkey, sizeof(setkey));
    ret = db_exec_cmd(db_handle, REDIS_ADD_ID_CMD, setkey, key, key_len, -1, NULL, 0);
    if (!IS_DB_RET_SUCCESS(ret))
        goto exit;

    /* update the hash table */

    hashkey_len = construct_hashkey_common(db_handle->id, key, key_len, hashkey, sizeof(hashkey));
    for (i=0; i<column_num; i++)
    { 
        cmd_len = 0;
        /* value type */
        memcpy(cmd_str+cmd_len, &columns[i].record_type, sizeof(columns[i].record_type));
        cmd_len += sizeof(columns[i].record_type);
        VNF_ASSERT(cmd_len < DB_CMD_DATA_MAX_LEN);

        /* value */
        RecordMetaInfo * rm = lookup_record_meta_table(columns[i].record_type);
        cmd_len += rm->pack(columns[i].record, (uint8_t*)(cmd_str+cmd_len));

        VNF_ASSERT(cmd_len < DB_CMD_DATA_MAX_LEN*3/4);

        ret = db_exec_cmd(db_handle, REDIS_WRITE_CMD, NULL, hashkey, hashkey_len, columns[i].column_id, cmd_str, cmd_len);

        
        if (!IS_DB_RET_SUCCESS(ret))
            goto exit;
    } 

    ret = db_exec_cmd(db_handle, REDIS_BATCH_END_CMD, NULL, hashkey, hashkey_len, -1, NULL, 0);

 exit:

    if (ret != DB_SUCCESS)
    {
        db_state_handle_error(db_handle);
    }

    return ret;
}

db_error_t db_delete_single_row(db_handle_t *db_handle, 
                                uint8_t* key, size_t key_len)
{
    VNF_ASSERT(key != NULL);
    VNF_ASSERT(key_len != 0);
    db_error_t ret = DB_SUCCESS;
    uint8_t setkey[128];
    uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
    int hashkey_len = 0;

    if(db_handle == NULL) {
        return DB_HANDLE_INVALID;
    }

    // construct the name of the keyset
    construct_setkey_common(db_handle->id, setkey, sizeof(setkey));
    ret = db_exec_cmd(db_handle, REDIS_DEL_ID_CMD, setkey, key, key_len, -1, NULL, 0);
    if (!IS_DB_RET_SUCCESS(ret))
        goto exit;

    hashkey_len = construct_hashkey_common(db_handle->id, key, key_len, hashkey, sizeof(hashkey));
    ret = db_exec_cmd(db_handle, REDIS_DEL_CMD, NULL, hashkey, hashkey_len, -1, NULL, 0);
    if (!IS_DB_RET_SUCCESS(ret))
        goto exit;

 exit:
    
    if (ret != DB_SUCCESS)
    {
        db_state_handle_error(db_handle);
    }

    return ret;
}

// Erstwhile db_delete_all_row() with FLUSHDB
// we can't do this anymore - the DB can have multiple tables
db_error_t db_delete_all(db_handle_t *db_handle)
{
    db_error_t ret = DB_SUCCESS;

    if(db_handle == NULL) {
        return DB_HANDLE_INVALID;
    }

    ret = db_exec_cmd(db_handle, REDIS_FLUSH_CMD, NULL, NULL, 0, -1, NULL, 0);
  
    if (ret != DB_SUCCESS)
    {
        db_state_handle_error(db_handle);
    }

    return ret;
}

db_error_t db_delete_all_row(db_handle_t *db_handle)
{
    db_error_t ret = DB_SUCCESS;
    uint8_t setkey[128];

    if(db_handle == NULL) {
        return DB_HANDLE_INVALID;
    }

#ifdef DEBUG_DB
    printf("in %s, delete_all_rows_in_progress=%d\n", __FUNCTION__, db_handle->delete_all_rows_in_progress);
#endif

    if (db_handle->delete_all_rows_in_progress)
        return DB_ERROR_DELETE_ALL_IN_PROGRESS;

    db_handle->delete_all_rows_in_progress = 1;
    // construct the name of the keyset
    construct_setkey_common(db_handle->id, setkey, sizeof(setkey));
    ret = db_exec_cmd(db_handle, REDIS_GET_ALL_IDS_CMD, setkey, NULL, 0, -1, NULL, 0);

    if (ret != DB_SUCCESS)
    {
        db_state_handle_error(db_handle);
    }

    return ret;
}

db_error_t db_read_all(db_handle_t *db_handle)
{
    db_error_t ret = DB_SUCCESS;
    uint8_t setkey[128];

    if(db_handle == NULL) {
        return DB_HANDLE_INVALID;
    }

#ifdef DEBUG_DB
    printf("in %s, full_read_in_progress=%d\n", __FUNCTION__, db_handle->full_read_in_progress);
#endif

    if (db_handle->full_read_in_progress)
        return DB_ERROR_FULL_READ_IN_PROGRESS;

    db_handle->full_read_in_progress = 1;
    // construct the name of the keyset
    construct_setkey_common(db_handle->id, setkey, sizeof(setkey));
    ret = db_exec_cmd(db_handle, REDIS_GET_ALL_IDS_CMD, setkey, NULL, 0, -1, NULL, 0);
    
    if (ret != DB_SUCCESS)
    {
        db_state_handle_error(db_handle);
    }

    return ret;
}

db_error_t db_become_master(char *ip)
{
    db_error_t ret = DB_SUCCESS;
    FILE *fp = NULL, *fp1 = NULL;
    char one_line[256];
    char * temp = NULL, *temp1 = NULL;
    int changed = 0;

    fp = fopen(REDIS_CONF_FILE, "r");
    fp1 = fopen(REDIS_CONF_FILE_TEMP, "w+");

    if (fp == NULL)
        return DB_ERROR_FILE_OP;

    while (1)
    {
        temp = fgets(one_line, sizeof(one_line), fp);
        if (temp == NULL)
            break;
        
        if (strstr(temp, "slaveof") != NULL)
        {
            temp1 = strstr(temp, "#");
            if((temp1 == NULL) || (strstr(temp1, "slaveof") == NULL)) {
                snprintf(one_line, sizeof(one_line), "# slaveof <masterip> <masterport>\n");
                fputs(one_line, fp1);
                changed = 1;
            }
        }
        else if (strncasecmp(temp, "bind", 4) == 0)
        {
            snprintf(one_line, sizeof(one_line), "bind %s 127.0.0.1\n", ip);
            fputs(one_line, fp1);
        }
        else
        {
            fputs(one_line, fp1);
        }

        one_line[0] = '\0';
    }
    
    fclose(fp);
    fclose(fp1);

    if(changed) {
        system("cp " REDIS_CONF_FILE_TEMP " " REDIS_CONF_FILE_MASTER);
        system("restart casa-redis role=master");    
    }

    return ret;
}

db_error_t db_slave_of(char *ip)
{
    VNF_ASSERT(ip != NULL);
    VNF_ASSERT(strlen(ip) != 0);

    db_error_t ret = DB_SUCCESS;
    FILE *fp = NULL, *fp1 = NULL;
    char one_line[256];
    char * temp = NULL;

    fp = fopen(REDIS_CONF_FILE, "r");
    fp1 = fopen(REDIS_CONF_FILE_TEMP, "w+");

    if (fp == NULL)
        return DB_ERROR_FILE_OP;

    while (1)
    {
        temp = fgets(one_line, sizeof(one_line), fp);
        if (temp == NULL)
            break;
        
        if (strstr(temp, "slaveof") != NULL)
        {
            snprintf(one_line, sizeof(one_line), "slaveof %s 6379\n", ip);
            fputs(one_line, fp1);
        }
        else
        {
            fputs(one_line, fp1);
        }

        one_line[0] = '\0';
    }
    
    fclose(fp);
    fclose(fp1);

    system("cp " REDIS_CONF_FILE_TEMP " " REDIS_CONF_FILE_SLAVE);
    system("restart casa-redis role=slave");    

    return ret;
}

db_error_t db_bind_interface(char *ip)
{
    VNF_ASSERT(ip != NULL);
    VNF_ASSERT(strlen(ip) != 0);

    db_error_t ret = DB_SUCCESS;
    FILE *fp = NULL, *fp1 = NULL;
    char one_line[256];
    char * temp = NULL, *temp1 = NULL, *temp2 = NULL;

    fp = fopen(REDIS_CONF_FILE, "r");
    fp1 = fopen(REDIS_CONF_FILE_TEMP, "w+");

    if (fp == NULL)
        return DB_ERROR_FILE_OP;

    while (1)
    {
        temp = fgets(one_line, sizeof(one_line), fp);
        if (temp == NULL)
            break;
        
        if (strncasecmp(temp, "bind", 4) == 0)
        {
            snprintf(one_line, sizeof(one_line), "bind %s 127.0.0.1\n", ip);
            fputs(one_line, fp1);
        }
        else
        {
            fputs(one_line, fp1);
        }

        one_line[0] = '\0';
    }
    
    fclose(fp);
    fclose(fp1);

    system("cp " REDIS_CONF_FILE_TEMP " " REDIS_CONF_FILE);
    system("restart casa-redis");

    return ret;
}

static void db_single_row_callback_fn(redisAsyncContext *ctx, void *reply_returned, void *privdata)
{
    int i;
    db_column_t *column;
    db_column_t columns[DB_MAX_COLUMNS];
    db_handle_t *db_handle = (db_handle_t*) ctx->data;
    db_columns_cmd_data_t *cmd = (db_columns_cmd_data_t *) privdata;
    redisReply *reply = (redisReply *)reply_returned;
    int num_columns;

    //printf("%s: \n", __FUNCTION__);

    if (reply == NULL 
            || reply->type != REDIS_REPLY_ARRAY
            || reply->elements % 2)
    {
        db_handle->read_cb(DB_ERROR, db_handle->cb_data, cmd->key+DB_CMD_HASHKEY_TAG_LEN, cmd->key_len-DB_CMD_HASHKEY_TAG_LEN, 0, 0); 
        g_slice_free(db_columns_cmd_data_t, cmd);
        return;
    }

    column = &columns[0];
    for (i=0; i < reply->elements; i+=2) 
    {
        redisReply *fieldReply, *valueReply;
        fieldReply = reply->element[i];
        valueReply = reply->element[i+1];

        memcpy(&column->column_id, fieldReply->str, fieldReply->len);
        column->record_type = *((int*)(valueReply->str));

        RecordMetaInfo *rm = lookup_record_meta_table(column->record_type);
        int record_len = valueReply->len-sizeof(column->record_type);
        uint8_t *record = (uint8_t *)valueReply->str + sizeof(column->record_type);
        column->record = (ProtobufCMessage*)(rm->unpack(NULL, record_len, record));

        column++;
    }

    num_columns = reply->elements/2;

    db_handle->read_cb(DB_SUCCESS, db_handle->cb_data, 
                       cmd->key+DB_CMD_HASHKEY_TAG_LEN, cmd->key_len-DB_CMD_HASHKEY_TAG_LEN, columns, num_columns); 
                
    for (i=0; i < num_columns; i++)
    {
        column = &columns[i];
        RecordMetaInfo *rm = lookup_record_meta_table(column->record_type);
        rm->free_unpacked(column->record, NULL);
    }

    g_slice_free(db_columns_cmd_data_t, cmd);
}

static db_error_t db_read_single_row_ex(
                         db_handle_t *handle, 
                         uint8_t* key, 
                         size_t key_len, 
                         db_notify_entry_t *entry)
{
    VNF_ASSERT(key != NULL);
    VNF_ASSERT(key_len != 0);
    uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
    int hashkey_len = 0;
    int ret;
    db_handle_t *db_handle;

    db_handle = (entry) ? db_handle_notify_data : handle;

    if (db_handle->conn_state != DB_CONNECTED)
        return DB_RETRY;

    db_columns_cmd_data_t *cmd = (db_columns_cmd_data_t *) g_slice_new0(db_columns_cmd_data_t);

    cmd->entry = entry;

    if (!entry)
    {
        hashkey_len = construct_hashkey_common(db_handle->id, key, key_len, hashkey, sizeof(hashkey));
        cmd->key_len = hashkey_len;
        memcpy(cmd->key, hashkey, hashkey_len);
    }
    else
    {
        cmd->key_len = key_len;
        memcpy(cmd->key, key, key_len);
    }

    ret = redisAsyncCommand(db_handle->redis_ctx, db_single_row_callback_fn, 
            cmd, "%s %b", REDIS_GET_SINGLE_ROW_CMD, cmd->key, cmd->key_len);

    return (ret == REDIS_OK) ? DB_SUCCESS : DB_ERROR;
}

db_error_t db_read_single_row(db_handle_t *db_handle, uint8_t* key, size_t key_len)
{
    VNF_ASSERT(key != NULL);
    VNF_ASSERT(key_len != 0);
    uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
    int hashkey_len = 0;

    int ret;
    db_columns_cmd_data_t *cmd = (db_columns_cmd_data_t *) g_slice_new0(db_columns_cmd_data_t);

    hashkey_len = construct_hashkey_common(db_handle->id, key, key_len, hashkey, sizeof(hashkey));

    cmd->key_len = hashkey_len;
    memcpy(cmd->key, hashkey, hashkey_len);

    ret = redisAsyncCommand(db_handle->redis_ctx, db_single_row_callback_fn, 
            cmd, "%s %b", REDIS_GET_SINGLE_ROW_CMD, hashkey, hashkey_len);

    return (ret == REDIS_OK) ? DB_SUCCESS : DB_ERROR;
}


static void db_single_row_columns_callback_fn(redisAsyncContext *ctx, void *reply_returned, void *privdata)
{
    int i;
    db_column_t *column;
    db_column_t columns[DB_MAX_COLUMNS];
    db_handle_t *db_handle = (db_handle_t*) ctx->data;
    redisReply *reply = (redisReply *)reply_returned;
    db_columns_cmd_data_t *cmd = (db_columns_cmd_data_t *) privdata;

    if (reply == NULL 
            || reply->type != REDIS_REPLY_ARRAY 
            || reply->elements != cmd->num_columns)
    {
        db_handle->read_cb(DB_ERROR, db_handle->cb_data, cmd->key+DB_CMD_HASHKEY_TAG_LEN, cmd->key_len-DB_CMD_HASHKEY_TAG_LEN, 0, 0); 
        g_slice_free(db_columns_cmd_data_t, cmd);
        return;
    }

    int real_num_columns = 0;
    column = &columns[0];
    for (i=0; i < reply->elements; i++) 
    {
        redisReply *rp = reply->element[i];

        if (rp && rp->len > 0 && rp->type == REDIS_REPLY_STRING)
        {
            column->column_id = cmd->column_id[i];
            column->record_type = *((int*)(rp->str));
            RecordMetaInfo *rm = lookup_record_meta_table(column->record_type);

            int record_len = rp->len-sizeof(column->record_type);
            uint8_t *record = (uint8_t *)rp->str + sizeof(column->record_type);

            column->record = (ProtobufCMessage*)(rm->unpack(NULL, record_len, record));
            column++;
            real_num_columns++;
        }
    }

    db_handle->read_cb(DB_SUCCESS, db_handle->cb_data, 
                       cmd->key+DB_CMD_HASHKEY_TAG_LEN, cmd->key_len-DB_CMD_HASHKEY_TAG_LEN,
                       columns, real_num_columns); 
                
    for (i=0; i < real_num_columns; i++)
    {
        column = &columns[i];
        RecordMetaInfo *rm = lookup_record_meta_table(column->record_type);
        rm->free_unpacked(column->record, NULL);
    }

    g_slice_free(db_columns_cmd_data_t, cmd);
}

static db_error_t db_read_single_row_columns_ex(db_handle_t *handle,
                                      uint8_t* key, size_t key_len,
                                      db_column_t *columns, size_t num_columns,
                                      db_notify_entry_t *entry)
{
    VNF_ASSERT(key != NULL);
    VNF_ASSERT(key_len != 0);

    int ret;
    int i;
    int argc;
    const char *argv[2 + DB_MAX_COLUMNS];
    size_t  argvlen[2 + DB_MAX_COLUMNS];
    db_columns_cmd_data_t *cmd;
    uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
    int hashkey_len = 0;
    db_handle_t *db_handle;

    db_handle = (entry) ? db_handle_notify_data : handle;

    if (db_handle->conn_state != DB_CONNECTED)
        return DB_RETRY;

    if (num_columns == 0 || columns == NULL)
        return DB_ERROR;

    cmd = (db_columns_cmd_data_t *) g_slice_new0(db_columns_cmd_data_t);

    cmd->entry = entry;
    cmd->num_columns = num_columns;
    if (!entry)
    {
        hashkey_len = construct_hashkey_common(db_handle->id, key, key_len, hashkey, sizeof(hashkey));
        cmd->key_len = hashkey_len;
        memcpy(cmd->key, hashkey, hashkey_len);
    }
    else
    {
        cmd->key_len = key_len;
        memcpy(cmd->key, key, key_len);
    }

    for (i=0; i < num_columns; i++)
        cmd->column_id[i] = columns[i].column_id;

    argc = 0;
    argv[argc] = REDIS_GET_SINGLE_ROW_COLUMNS_CMD;
    argvlen[argc++] = strlen(REDIS_GET_SINGLE_ROW_COLUMNS_CMD);
    argv[argc] = cmd->key;
    argvlen[argc++] = cmd->key_len;

    for (i=0; i < num_columns; i++) 
    {
        argv[argc] = (char *) &columns[i].column_id; 
        argvlen[argc++] = sizeof(columns[i].column_id);
    }

    ret = redisAsyncCommandArgv(db_handle->redis_ctx, db_single_row_columns_callback_fn, cmd, argc, argv, argvlen);

    return (ret == REDIS_OK) ? DB_SUCCESS : DB_ERROR;
}

db_error_t db_read_single_row_columns(db_handle_t *db_handle,
                                      uint8_t* key, size_t key_len,
                                      db_column_t *columns, size_t num_columns)
{
    VNF_ASSERT(key != NULL);
    VNF_ASSERT(key_len != 0);

    int ret;
    int i;
    int argc;
    const char *argv[2 + DB_MAX_COLUMNS];
    size_t  argvlen[2 + DB_MAX_COLUMNS];
    db_columns_cmd_data_t *cmd;
    uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
    int hashkey_len = 0;

    if (num_columns == 0 || columns == NULL)
        return DB_ERROR;

    cmd = (db_columns_cmd_data_t *) g_slice_new0(db_columns_cmd_data_t);
    hashkey_len = construct_hashkey_common(db_handle->id, key, key_len, hashkey, sizeof(hashkey));
    cmd->key_len = hashkey_len;
    memcpy(cmd->key, hashkey, hashkey_len);
    cmd->num_columns = num_columns;

    for (i=0; i < num_columns; i++)
        cmd->column_id[i] = columns[i].column_id;

    argc = 0;
    argv[argc] = REDIS_GET_SINGLE_ROW_COLUMNS_CMD;
    argvlen[argc++] = strlen(REDIS_GET_SINGLE_ROW_COLUMNS_CMD);
    argv[argc] = hashkey;
    argvlen[argc++] = hashkey_len;

    for (i=0; i < num_columns; i++) 
    {
        argv[argc] = (char *) &columns[i].column_id; 
        argvlen[argc++] = sizeof(columns[i].column_id);
    }

    ret = redisAsyncCommandArgv(db_handle->redis_ctx, db_single_row_columns_callback_fn, cmd, argc, argv, argvlen);

    return (ret == REDIS_OK) ? DB_SUCCESS : DB_ERROR;
}

static void db_integer_callback_fn(redisAsyncContext *ctx, void *reply_returned, void *privdata)
{
    db_integer_cmd_data_t *cmd = privdata;
    db_read_integer_callback_func_t cb = cmd->cb_func;
    redisReply *reply = (redisReply *)reply_returned;

    if (reply == NULL || reply->type != REDIS_REPLY_INTEGER)
        cb(DB_ERROR, cmd->cb_arg, 0);
    else
        cb(DB_SUCCESS, cmd->cb_arg, reply->integer);

    g_slice_free(db_integer_cmd_data_t, cmd);
}

db_error_t db_get_total_row_number(db_handle_t *db_handle, 
                                db_read_integer_callback_func_t cb,
                                void *cb_arg)
{
    int ret;
    db_integer_cmd_data_t *cmd = g_slice_new0(db_integer_cmd_data_t);
    uint8_t setkey[128];

    cmd->cb_func = cb;
    cmd->cb_arg = cb_arg;
    //printf("%s\n", __FUNCTION__);

    // construct the name of the keyset
    construct_setkey_common(db_handle->id, setkey, sizeof(setkey));
    ret = redisAsyncCommand(db_handle->redis_ctx, db_integer_callback_fn, cmd, "%s %b", 
                        REDIS_SCARD_CMD, setkey, strlen(setkey));

    return (ret == REDIS_OK) ? DB_SUCCESS : DB_ERROR;
}

static void null_write_cb(db_error_t err, void *cb_arg, uint8_t * key, uint8_t key_len)
{
}

static void null_read_cb (db_error_t err,
                   void *cb_arg, 
                   uint8_t *key, uint8_t key_len, 
                   db_column_t *reply_data, 
                   size_t reply_data_num)
{
}

static void null_read_complete_cb(db_error_t err, void *cb_arg)
{
}

static void null_ctrl_cb(void *cb_arg, db_error_t err)
{
}

static void db_init_internal(void)
{
    static int initialized = 0;

    if (initialized)
        return;

    initialized = 1;

    record_meta_table_init();

    if (!HaChRowMsgFns)
    {
        HaChRowMsgFns = lookup_record_meta_table(RECORDID(HA, CH_ROW_MSG));
        VNF_ASSERT(HaChRowMsgFns);
    }

    if (!db_handle_queue)
    {
        db_handle_queue = g_queue_new();
    }

    if (!db_handle_queue_sync)
    {
        db_handle_queue_sync = g_queue_new();
    }

    if (!notify_entry_queue)
    {
        notify_entry_queue = g_queue_new();
    }

    if (!db_handle_notify_control)
    {
        db_open_ex(0, &db_handle_notify_control, 0, 
                null_ctrl_cb, null_write_cb, null_read_cb, null_read_complete_cb, 
                DB_CONNECT_LOCAL);
    }

    if (!db_handle_notify_data)
    {
        db_open_ex(0, &db_handle_notify_data, 0, 
                null_ctrl_cb, null_write_cb, null_read_cb, null_read_complete_cb, 
                DB_CONNECT_LOCAL);
    }
}

/** This function must be called before all other db api functions, it is called at vnfha_api_init*/
void db_api_init(void)
{
    //printf("%s\n", __FUNCTION__);

    static int initialized = 0;

    if (initialized)
        return;

    initialized = 1;

    db_init_internal();
}

static void db_connect_remote_server(gpointer data, gpointer user_data)
{
    db_handle_t *db_handle = (db_handle_t *) data;

    if (db_handle->connect_mode == DB_CONNECT_MASTER)
    {
        redisAsyncCommand(db_handle->redis_ctx, NULL, (void*)(db_handle), "quit");
        // db_disconnect_fn() will do db_reconnect.
    }
}

static void db_queue_noti_row(db_handle_t *db_handle, uint8_t *key, size_t key_len)
{
    db_recover_noti_t *entry;
    int len = g_queue_get_length(notify_entry_queue);
    GQueue *queue = db_handle->recover_noti_queue;

    while (len--)
    {
        entry = (db_recover_noti_t *) g_queue_pop_head(queue);
        if (!entry)
            break;
        
        g_queue_push_tail(queue, entry);

        if (entry->key_len == key_len && !memcmp(entry->key, key, key_len))
            return;
    }

    entry = g_slice_new0(db_recover_noti_t);
    entry->key_len = key_len;
    memcpy(entry->key, key, key_len);
    g_queue_push_tail(queue, entry);
}


static void db_notification_cb(redisAsyncContext *ctx, void *reply, void *privdata)
{
    redisReply *rep = (redisReply *) reply;
    db_notify_entry_t *entry = (db_notify_entry_t *) privdata;
    db_handle_t *db_handle = (db_handle_t *) ctx->data;
    HaChRowMsg *msg;
    int i;
    db_column_t columns[DB_MAX_COLUMNS];


    if (db_handle->closing)
        return;

    if (!rep)
    {
        //printf("%s: null reply \n", __FUNCTION__);
        return;
    }

    if (rep->type != REDIS_REPLY_ARRAY)
    {
        //printf("%s: Bad type = %d.\n", __FUNCTION__, rep->type);
        return;
    }

    if (rep->elements != 3)
    {
        //printf("%s: Bad number of elements = %d.\n", __FUNCTION__, rep->elements);
        return;
    }

    if (!memcmp("subscribe", rep->element[0]->str, rep->element[0]->len))
    {
        /*
        printf("%s: \"subscribe\" for ch:\n", __FUNCTION__);
        print_hex(rep->element[1]->str, rep->element[1]->len);
        */

        return;
    }

    if (!memcmp("unsubscribe", rep->element[0]->str, rep->element[0]->len))
    {
        //printf("%s: \"unsubscribe\" for ch:\n", __FUNCTION__);
        //print_hex(rep->element[1]->str, rep->element[1]->len);

        return;
    }

    if (memcmp("message", rep->element[0]->str, rep->element[0]->len))
    {
        /*
        printf("%s: \"message\" for ch:\n", __FUNCTION__);
        print_hex(rep->element[1]->str, rep->element[1]->len);
        */

        return;
    }

    /*
    printf("%s: \"pmessage\" for ch:\n", __FUNCTION__);
    print_hex(rep->element[1]->str, rep->element[1]->len);
    printf("%s: data:\n", __FUNCTION__);
    print_hex(rep->element[2]->str, rep->element[2]->len);
    */


    msg = HaChRowMsgFns->unpack(NULL, rep->element[2]->len, rep->element[2]->str);
    if (!msg)
    {
        printf("%s: failed to unpack.\n", __FUNCTION__);
        return;
    }

    if (db_handle && db_handle->recovering)
    {
        db_queue_noti_row(db_handle, msg->key.data, msg->key.len);
        goto exit;
    }

    switch (msg->event) {
    case HA_CH_ROW_MSG__EVENT__DELETE:
        //printf("%s: CL: type=DELETE, keylen=%d\n", __FUNCTION__, msg->key.len);
        entry->notify_cb(DB_SUCCESS,
                        entry->db_id,
                        entry->notify_cb_arg,
                        msg->key.data + DB_CMD_HASHKEY_TAG_LEN,
                        msg->key.len - DB_CMD_HASHKEY_TAG_LEN,
                        NULL,
                        0);
        break;

    case HA_CH_ROW_MSG__EVENT__COMPLETE_UPDATE:
        //printf("%s: CL: type=COMPLETE_UPDATE, keylen=%d\n", __FUNCTION__, msg->key.len);
        db_read_single_row_ex(NULL, msg->key.data, msg->key.len, entry);
        break;

    case HA_CH_ROW_MSG__EVENT__PARTIAL_UPDATE:
        //printf("%s: CL: type=PARTIAL_UPDATE, keylen=%d, n_column_id=%d\n", __FUNCTION__, msg->key.len, msg->n_column_id);

        for (i=0; i < msg->n_column_id; i++)
        {
            columns[i].column_id = msg->column_id[i];
        }

        db_read_single_row_columns_ex(db_handle, msg->key.data, msg->key.len,
                                      columns, msg->n_column_id, entry);
        break;

    default:
        break;
    }

exit:
    HaChRowMsgFns->free_unpacked(msg, NULL);
}

static void db_subscribe_row_change(db_notify_entry_t *entry)
{
    uint8_t ch[32];
    size_t ch_len;

    if (db_handle_notify_control->conn_state != DB_CONNECTED)
        return;

    if (local_is_master && entry->mode == DB_NOTIFY_STANDBY_ONLY)
        return;

    if (!local_is_master && entry->mode == DB_NOTIFY_ACTIVE_ONLY)
        return;

    ch_len = sprintf(ch, "%s%u", DB_ROW_CHANGE_NOTIFICATION_CH_TAG, entry->db_id);
    redisAsyncCommand(db_handle_notify_control->redis_ctx, db_notification_cb, entry, "SUBSCRIBE %b", ch, ch_len);
}

static void db_unsubscribe_row_change(db_notify_entry_t *entry)
{
    uint8_t ch[32];
    size_t ch_len;

    if (db_handle_notify_control->conn_state != DB_CONNECTED)
        return;

    ch_len = sprintf(ch, "%s%u", DB_ROW_CHANGE_NOTIFICATION_CH_TAG, entry->db_id);
    redisAsyncCommand(db_handle_notify_control->redis_ctx, db_notification_cb, entry, "UNSUBSCRIBE %b", ch, ch_len);
}


static void db_master_redo_notification(gpointer data, gpointer user_data)
{
    db_notify_entry_t *entry = (db_notify_entry_t *) data;

    if (entry->removal_time)
        return;

    if (entry->mode == DB_NOTIFY_STANDBY_ONLY)
    {
        db_unsubscribe_row_change(entry);
    }

    if (entry->mode == DB_NOTIFY_ACTIVE_ONLY)
    {
        db_subscribe_row_change(entry);
    }
}

static void db_init_new_master(gpointer data, gpointer user_data)
{
    db_handle_t *db_handle = (db_handle_t *) data;

    if (db_handle->connect_mode == DB_CONNECT_MASTER)
    {
        redisAsyncCommand(db_handle->redis_ctx, NULL, (void*)(db_handle), "quit");
        // db_disconnect_fn() will do db_reconnect.
        return;
    }
}

static void db_slave_redo_notification(gpointer data, gpointer user_data)
{
    db_notify_entry_t *entry = (db_notify_entry_t *) data;

    if (entry->removal_time)
        return;

    if (entry->mode == DB_NOTIFY_STANDBY_ONLY)
    {
        db_subscribe_row_change(entry);
    }

    if (entry->mode == DB_NOTIFY_ACTIVE_ONLY)
    {
        db_unsubscribe_row_change(entry);
    }
}


static void db_init_new_slave(gpointer data, gpointer user_data)
{
    db_handle_t *db_handle = (db_handle_t *) data;

    if (db_handle->connect_mode == DB_CONNECT_MASTER)
    {
        redisAsyncCommand(db_handle->redis_ctx, NULL, (void*)(db_handle), "quit");
        // db_disconnect_fn() will do db_reconnect.
        return;
    }
}



void db_new_status(int type)
{
    switch (type)
    {
        case DB_NEW_STATUS_REMOTE_SERVER_IP:
        {
            g_queue_foreach(db_handle_queue, db_connect_remote_server, 0);

            break;
        }

        case DB_NEW_STATUS_MASTER:
        {
            g_queue_foreach(notify_entry_queue, db_master_redo_notification, 0);
            g_queue_foreach(db_handle_queue, db_init_new_master, 0);

            break;
        }

        case DB_NEW_STATUS_SLAVE:
        {
            g_queue_foreach(notify_entry_queue, db_slave_redo_notification, 0);
            g_queue_foreach(db_handle_queue, db_init_new_slave, 0);

            break;
        }
    }
}

