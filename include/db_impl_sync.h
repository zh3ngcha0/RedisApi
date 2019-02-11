#ifndef __DB_IMPL_SYNC_H__
#define __DB_IMPL_SYNC_H__

#include "vnf_assert.h"
#include <stdbool.h>

#include "db_common.h"

#define DB_HANDLE_SYNC_MAGIC 0x6789

typedef int db_sync_magic_t;

// redisCommandArgv() argvs
//
// arg0 - command
// arg1 - key
// arg2 - keylen
// arg3 - field1
// arg4 - val1
// arg5 - field2
// arg6 - val2
// ..
// ..

#define DB_SYNC_MAX_FIELDS      256 // max columns
#define DB_SYNC_KEY_KEYLEN_ARGS 2 // hash key and keylen
#define DB_SYNC_COMMAND_ARG     1 // redis command for HASH tables

#define DB_SYNC_MAX_ARGS ((DB_SYNC_COMMAND_ARG)+(DB_SYNC_KEY_KEYLEN_ARGS)+((DB_SYNC_MAX_FIELDS)*2))


#define DB_SYNC_CMD_MAX_LEN     64      // redis command without param 
#define DB_SYNC_CMD_MAX_KEY_LEN 1024    // key len max size
#define DB_SYNC_MAX_KEYTYPE     16 
#define DB_SYNC_MAX_COLUMNS     DB_SYNC_MAX_FIELDS
#define DB_SYNC_MAX_ROWS        1024
#define DB_SYNC_MAX_COLUMN_SIZE 8192

#define DB_SYNC_REPLY_STR_MAX_LEN 2048

#define DB_SYNC_CMD_TIMEOUT_MS  5000

#define DB_SYNC_MAX_SCAN_COUNT  1024 // allow upto this number to queue the responses

typedef struct db_sync_cmd_data_t {
  char cmd[DB_SYNC_CMD_MAX_LEN]; // refers to REDIS command string without args
  char cmdarg[DB_SYNC_CMD_MAX_LEN]; // refers to REDIS arg string that follows the command 
  uint8_t *key; // key supplied by application
  size_t key_len; // redis type 
  uint32_t argc; // required by redisCommandArgv
  const char *argv[DB_SYNC_MAX_ARGS];
  size_t argvlen[DB_SYNC_MAX_ARGS];
  struct db_sync_cmd_data_t **trans_cmds; // points to commands in a transaction multi/exec block
  uint32_t trans_cmd_count; // number of commands in a transcation 
  redisReply *trans_reply; // points to reply of transaction exec command
  bool process_reply; // set to true if redidReply requires additional processing
  redisReply *reply; // if set, needs to freed redisReplyFree after processing 
} db_sync_cmd_data_t;

typedef struct db_handle_sync_param_t {
  uint32_t timeout_ms;
  uint32_t num_keytypes; // key0 primary key1..N secondary keys for lookup
  uint32_t keytype_column_ids[DB_SYNC_MAX_KEYTYPE]; // colN contains the value of keyN   
  char     server_ip[48]; //in case of either ipv4 or ipv6
  uint32_t server_port; 
  uint32_t max_row_count; //  maximum number of rows to expect in this table
  uint32_t max_column_size; // maximum size of the data in a column
  uint32_t max_columns; // maximum number of columns to expect in this table
} db_handle_sync_param_t;

typedef struct db_handle_sync_t {
  db_sync_magic_t magic;
  db_id_t id;
  redisContext *redis_ctx;

  db_handle_sync_param_t params;
  bool start_transaction;
    
  bool      scan_in_progress;
  uint64_t  scan_cursor; 
  uint32_t  scan_replies_pi;
  uint32_t  scan_replies_ci;
  uint8_t  *scan_reply_keys[DB_SYNC_MAX_SCAN_COUNT+1];
  uint32_t  scan_reply_keylen[DB_SYNC_MAX_SCAN_COUNT+1];

  // perf numbers
  long req_ts_us;
  long resp_ts_us;
  long max_read_row_us;
  long min_read_row_us;
  long max_write_row_us;
  long min_write_row_us;
  long max_read_col_us;
  long min_read_col_us;
  long max_write_col_us;
  long min_write_col_us;

  // may be some stats - req/res
  uint32_t get_cmd_count;
  uint32_t set_cmd_count;
  uint32_t append_cmd_count;
  uint32_t get_reply_count;
  uint32_t free_reply_count;

  db_connect_mode_t connect_mode;
} db_handle_sync_t;

typedef struct db_sync_get_reply_t {
  redisReply *reply;
  db_sync_cmd_data_t *cmd;
} db_sync_get_reply_t;

#define VNF_DB_HANDLE_SYNC_ASSERT(h) (VNF_ASSERT(!h) && h->magic == DB_HANDLE_SYNC_MAGIC)

#define DBTBL_GROUP_ID(tblid) ((tblid) >> (DBTBL_TYPE_BITS + DBINST_BITS))
#define DBTBL_TYPE_ID(tblid)  (((tblid) >> DBINST_BITS) & ~(1 << DBTBL_TYPE_BITS))

#endif
