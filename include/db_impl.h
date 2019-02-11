#ifndef __DATABASE_IMPL_H__
#define __DATABASE_IMPL_H__

#include "vnf_assert.h"
#include <hiredis/async.h>
#include <stdbool.h>
#include <glib.h>
#include <glib/gqueue.h>

#define DB_HANDLE_MAGIC 0x5678

typedef int db_magic_t;

#define DB_CMD_MAX_LEN 256
#define DB_CMD_DATA_MAX_LEN 8192
#define DB_CMD_MAX_KEY_LEN 256
#define DB_REPLY_STR_MAX_LEN DB_CMD_DATA_MAX_LEN*2
#define DB_MAX_COLUMNS 16

#define REDIS_GET_ALL_IDS_CMD "smembers"
#define REDIS_GET_SINGLE_ROW_CMD "hgetall"
#define REDIS_WRITE_CMD "HMSET"
#define REDIS_ADD_ID_CMD "SADD"
#define REDIS_DEL_ID_CMD "SREM"
#define REDIS_DEL_CMD "DEL"
#define REDIS_FLUSH_CMD "flushdb"
#define REDIS_BATCH_START_CMD "multi"
#define REDIS_BATCH_END_CMD "exec"
#define REDIS_ID_SET_NAME "ids"
#define REDIS_GET_SINGLE_ROW_COLUMNS_CMD "HMGET"
#define REDIS_SCARD_CMD "SCARD"

#define REDIS_CONF_FILE "/etc/redis.conf"
#define REDIS_CONF_FILE_MASTER "/etc/redis.conf.master"
#define REDIS_CONF_FILE_SLAVE "/etc/redis.conf.slave"
#define REDIS_CONF_FILE_TEMP "/etc/redis.conf.temp"

// Update version if HASHKEY format changes
#define DB_API_VERSION 1
// HASHKEY FORMAT
// <HASHKEY_TAG> <KEY>
#define DB_CMD_HASHKEY_TAG_LEN 10 // 2 chars of db_api_version 8 chars for 4 byte tableid

typedef enum {
    DB_INIT = 0,
    DB_CONNECTING,
    DB_CONNECTED,
    DB_DISCONNECTED
} db_connect_state_t;

#define DB_ROW_CHANGE_NOTIFICATION_CH_TAG   "row@"

typedef struct {
    int number_of_rows;
} read_cb_state_t;

typedef enum {
    DB_CMD_STR1_ONLY = 1,
    DB_CMD_STR1_STR2,
    DB_CMD_WITH_KEY,
    DB_CMD_WITH_DATA,
} db_cmd_type_t;


typedef struct {
    db_cmd_type_t cmd_type;
    char cmd[DB_CMD_MAX_LEN];
    char cmd2[DB_CMD_MAX_LEN];
    uint8_t key[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
    size_t key_len;
    int column_id;
    char column_data[DB_CMD_DATA_MAX_LEN];
    size_t column_data_len;
} db_cmd_data_t;

typedef struct {
    uint8_t key[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
    size_t key_len;
} db_recover_noti_t;

typedef struct {
    uint8_t key[DB_CMD_MAX_KEY_LEN];
    size_t key_len;
    int32_t column_id[DB_MAX_COLUMNS];
    int num_columns;    
    struct db_notify_entry_s *entry;
} db_columns_cmd_data_t;

typedef struct {
    void *cb_func;
    void *cb_arg;
} db_integer_cmd_data_t;

typedef struct db_handle_t_s {
    db_magic_t magic;
    db_id_t id;
    void *cb_data;
    redisAsyncContext *redis_ctx;
    db_ctrl_callback_func_t ctrl_cb;
    db_write_callback_func_t write_cb;
    db_read_callback_func_t read_cb;
    db_read_complete_callback_func_t read_complete_cb;

    /* store pending cmd queue */
    GQueue *pending_cmd_queue;

    read_cb_state_t read_state;
    db_connect_state_t conn_state;
    bool full_read_in_progress;
    bool batch_cb_start;
    bool batch_cb_end;
    bool write_cmd_cb_received;
    bool delete_all_rows_in_progress;
    bool close_pending;
//    VNFTimer close_pending_timer;
//    VNFTimer reconnect_timer;
    int     reconnect_try_cnt;

    bool closing;
    db_connect_mode_t connect_mode;
    GQueue *recover_noti_queue;
    bool recovering;
    struct db_notify_entry_s *recover_noti_entry;
} db_handle_t;

typedef struct db_notify_entry_s {
    db_id_t db_id;
    db_notify_callback_func_t notify_cb;
    void *notify_cb_arg;
    db_notify_mode_t mode;
    uint32_t  removal_time;
    struct db_handle_s *db_handle;
} db_notify_entry_t;

#define DB_NEW_STATUS_REMOTE_SERVER_IP  0
#define DB_NEW_STATUS_MASTER            1 
#define DB_NEW_STATUS_SLAVE             2 

#define VNF_DB_HANDLE_ASSERT(h) (VNF_ASSERT(!h) && h->magic == DB_HANDLE_MAGIC)

#endif
