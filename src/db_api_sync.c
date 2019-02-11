#include "db_api.h"
#include "db_api_sync.h"
#include <glib.h>
#include "db_ha.h"
#include <string.h>

#include <stdlib.h>

#include "scheduler.h"

#include "record_table.h"
#include <hiredis/hiredis.h>
#include "record_ha.h"
#include "record_ha.pb-c.h"

#include "db_api_util.h"
#include "db_api_common.h"

// local methods declarations
static db_error_t connect_redis(db_handle_sync_t *db_handle);
static db_error_t disconnect_redis(db_handle_sync_t *db_handle);
static db_error_t reconnect_redis(db_handle_sync_t *db_handle);
static db_error_t exec_sync_cmd(db_handle_sync_t *db_handle, db_sync_cmd_data_t *cmd, bool retry);
static bool check_db_handle_valid(db_handle_sync_t *db_handle); 
static bool check_dbtbl_id_valid(db_id_t dbtbl_id);
static uint32_t construct_hashkey(db_handle_sync_t *db_handle, uint8_t *key, uint32_t key_len, uint8_t *hashkey, uint32_t hashkey_len);
uint32_t construct_setkey(db_handle_sync_t *db_handle, uint8_t *setkey, uint32_t setkey_len);
static db_error_t append_sync_cmd(db_handle_sync_t *db_handle, db_sync_cmd_data_t *cmd);
static db_error_t get_reply_sync_cmd(db_handle_sync_t *db_handle, db_sync_get_reply_t *get_reply, bool retry);
static db_error_t write_single_row_hset(db_handle_sync_t *db_handle,
                                        uint8_t* key, uint32_t key_len,
                                        db_column_t *columns, size_t num_columns);
static db_error_t write_single_row_hmset(db_handle_sync_t *db_handle,
                                         uint8_t* key, uint32_t key_len,
                                         db_column_t *columns, size_t num_columns);

static db_error_t start_transaction(db_handle_sync_t *db_handle);
static db_error_t exec_transaction(db_handle_sync_t *db_handle, bool process_reply);
static db_error_t discard_transaction(db_handle_sync_t *db_handle);
static void setup_db_params(db_handle_sync_t *db_handle, db_handle_sync_param_t *params);
static void setup_default_db_params(db_handle_sync_t *db_handle);
static void free_scan_replies(db_handle_sync_t *db_handle);
#if 0
static void print_data(uint8_t *buf, uint32_t len);
#endif

#define REDIS_SERVER_DEFAULT_IP    "127.0.0.1"
#define REDIS_SERVER_DEFAULT_PORT  6379

extern int local_is_master;
extern char remote_server_ip[];
extern RecordMetaInfo *HaChRowMsgFns;
extern GQueue *db_handle_queue_sync;
extern GQueue *db_handle_queue;

static void db_init_internal_sync(void)
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
}

db_error_t db_open_sync_ex(db_id_t dbtble_id, 
                            db_handle_sync_param_t *params, 
                            db_handle_sync_t **db_handle_out, 
                            db_connect_mode_t mode) 
{
  db_handle_sync_t *db_handle = NULL;
  db_sync_cmd_data_t cmd;

  db_init_internal_sync();

  if (!local_is_master 
        && mode == DB_CONNECT_MASTER
        && remote_server_ip[0] == '\0')
  {
    printf("%s: Remote server address is empty.\n", __FUNCTION__);
    return DB_ERROR;
  }

  *db_handle_out = db_handle;

  // check valid dbtble_id
  if (!check_dbtbl_id_valid(dbtble_id)) {
    return DB_CONN_FAILED;
  }

  db_handle = g_malloc0(sizeof(db_handle_sync_t));

  VNF_ASSERT(db_handle != NULL);

  memset(db_handle, 0, sizeof(db_handle_sync_t));

  db_handle->magic = DB_HANDLE_SYNC_MAGIC;
  db_handle->id = dbtble_id;
  db_handle->connect_mode = mode;

  setup_db_params(db_handle, params); 

  // connect to the REDIS server for this db
  if (connect_redis(db_handle) != DB_SUCCESS) {
    g_free(db_handle);
    return DB_CONN_FAILED;
  }

  // select the db in the REDIS server
  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  //Lian: use the dbtble_id directly to avoid conflict with other table access
  //sprintf(cmd.cmd, "select %d", DBTBL_GROUP_ID(dbtble_id));
  //
  // PK - DON'T USE SELECT - Not supported in cluster mode 
  // From REDIS documentation
  // Redis Cluster does not support multiple databases like the stand alone version of Redis. 
  // There is just database 0 and the SELECT command is not allowed
  // sprintf(cmd.cmd, "select %d", dbtble_id);
  // 
  // In standalone mode, must select 0  
  // - PK FIX ME verify the need of this call, and remove if not mandatory
  sprintf(cmd.cmd, "select 0");
  if (exec_sync_cmd(db_handle, &cmd, false) != DB_SUCCESS) {
    disconnect_redis(db_handle); 
    g_free(db_handle);
    return DB_CONN_FAILED;
  }

  *db_handle_out = db_handle;

  g_queue_push_tail(db_handle_queue_sync, db_handle);

  return DB_SUCCESS;
}

/*--------------------------------------------------
 *  Public API methods definitions below 
 *--------------------------------------------------*/

/*-------------------------------------------------------------------------------
 *
 * Method : db_open_sync
 *
 * Description : This opens a redis connection for an application to access records 
 * in a DBTable identified by dbtbl_id. An application must specify a "valid" dbtbl_id 
 * which comprises of the following: 
 *
 *                   16 bits       4 bits
 *  DB group id |  DB table id | DB instance id 
 *
 * The DB group id indetifies a REDIS DB instance. We would like to have no more than one 
 * REDIS DB instance per REDIS server. The REDIS DB_GROUP_CDB (value 0) will be assigned for 
 * all legacy CMTS CDB_ID_XXX tables.
 *
 * The DB table id indentifies specific DB Table that belongs to a DB group id. For each 
 * CMTS CDB_ID_XXX a unique table id will be assigned. 
 *
 * The DB Instance Id specifies the number of DB Tables of the same type and currently 
 * only once instance id 0 is supported. 
 *
 * If dbtbl_id is valid, this method opens a REDIS client connection to the REDIS DB idenfied 
 * by group id. 
 *
 * Arg list:
 * dbtbl_id   - dbtable id which includes grup, table type and instance id of a DBTBL instance
 *   Caller MUST specifiy a valid DBTBLID.
 *
 * param      - points to optional db_handle_sync_param which includes key and timeout values
 *   If NULL, default param values are used. 
 *
 * db_handle_out - The caller specifies a pointer to db_handle_sync_t pointer which is set to 
 *   a valid handle pointer if this call is successful.
 *   The caller specifies this handle for any future read/write operation to the DBTBL
 *   If this method fails, handle pointer value is set to NULL. 
 *
 *   The caller must call db_close_sync(db_handle) to free up the resources associated with the handle 
 *
 * Return:
 *   DB_SUCCESS on success and DB_CONN_FAILED if unable to open REDIS connection for the DBTBL.
 *
 * Notes:
 * A row in a table in REDIS is represented by a REDIS set and is added by SADD (or ZADD if sorted)
 *
 * The SADD/ZADD command is invoked with <dbtbl_id> as the set and <key> as the key which uniquely indetifies
 * a row in table.
 *
 * redis>sadd <dbtbl_id> <key>
 *
 * A column (a field in a row) is added as hash field/value pair to the key which has the form <dbtbl_id>:<key>.
 *
 * redis>hset dbtbl_id:key  columnIdX valueX 
 *
 * for instance : The legacy CDB_ID_CM Table is indexed by CM_ID which is the unique key
 *
 * When a row in this table is  added with key CM_ID value 100, the following command is run to 
 * add the key/row to the  
 * 
 * redis>sadd DB_TBL_ID_CDB_CM 100
 *
 * for instance 
 *
 * when the value of the macAddress and ipaddress of a CM is set for CM_ID 100.
 *
 * redis>hset DB_TBL_ID_CDB_CM:100  macAddressColumnId <macAddress> 
 * redis>hset DB_TBL_ID_CDB_CM:100  ipAddressColumnId <ipAddress> 
 *
 * when the value of the macAddress and ipaddress of a CM is set for CM_ID 200.
 *
 * redis>hset DB_TBL_ID_CDB_CM:200  macAddressColumnId <macAddress> 
 * redis>hset DB_TBL_ID_CDB_CM:200  ipAddressColumnId <ipAddress> 
 *
 * To retrieve all fields in a row (for instance CM_ID 100), the following command will be executed
 *
 * redis>hgetall DB_TBL_ID_CDB_CM:100 
 * this will provide the list of <field1> <value1> <field2> <value2> .....<fieldN> <valueN>
 *
 * To retrieve entire CDB_CM table the following command will be executed
 *
 * redis>smembers/sscan DB_TBL_ID_CDB_CM 
 * which will provide the list of CM_IDs 
 *
 * redis>hgetall DB_TBL_ID_CDB_CM:<CM_ID>
 * this will provide list of filed/value for a row
 *
 *-------------------------------------------------------------------------------*/
db_error_t db_open_sync(db_id_t dbtble_id, db_handle_sync_param_t *params, db_handle_sync_t **db_handle_out) 
{
  db_handle_sync_t *db_handle = NULL;
  db_sync_cmd_data_t cmd;

  //Get ready for packing and unpacking DB records
  record_meta_table_init(); 

  *db_handle_out = db_handle;

  // check valid dbtble_id
  if (!check_dbtbl_id_valid(dbtble_id)) {
    return DB_CONN_FAILED;
  }

  db_handle = g_malloc0(sizeof(db_handle_sync_t));

  VNF_ASSERT(db_handle != NULL);

  memset(db_handle, 0, sizeof(db_handle_sync_t));

  db_handle->magic = DB_HANDLE_SYNC_MAGIC;
  db_handle->id = dbtble_id;

  setup_db_params(db_handle, params); 

  // connect to the REDIS server for this db
  if (connect_redis(db_handle) != DB_SUCCESS) {
    g_free(db_handle);
    return DB_CONN_FAILED;
  }

  // select the db in the REDIS server
  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  //Lian: use the dbtble_id directly to avoid conflict with other table access
  //sprintf(cmd.cmd, "select %d", DBTBL_GROUP_ID(dbtble_id));
  //
  // PK - DON'T USE SELECT - Not supported in cluster mode 
  // From REDIS documentation
  // Redis Cluster does not support multiple databases like the stand alone version of Redis. 
  // There is just database 0 and the SELECT command is not allowed
  // sprintf(cmd.cmd, "select %d", dbtble_id);
  // 
  // In standalone mode, must select 0  
  // - PK FIX ME verify the need of this call, and remove if not mandatory
  sprintf(cmd.cmd, "select 0");
  if (exec_sync_cmd(db_handle, &cmd, false) != DB_SUCCESS) {
    disconnect_redis(db_handle); 
    g_free(db_handle);
    return DB_CONN_FAILED;
  }

  *db_handle_out = db_handle;

  return DB_SUCCESS;
}

db_error_t db_close_sync(db_handle_sync_t *db_handle)
{
  if (!check_db_handle_valid(db_handle)) {
    return DB_HANDLE_INVALID;
  }

  free_scan_replies(db_handle);
  disconnect_redis(db_handle); 
  g_free(db_handle);

  return DB_SUCCESS;
}

db_error_t db_write_single_column_sync(db_handle_sync_t *db_handle,
                                       uint8_t* key,
                                       uint32_t key_len,
                                       db_column_t* column)
{
  db_sync_cmd_data_t cmd;     
  uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
  int hashkey_len = 0;
  uint8_t val[DB_CMD_DATA_MAX_LEN];
  int vallen = 0;
  uint8_t setkey[128];
  db_error_t ret = DB_SUCCESS;

  VNF_ASSERT(key);
  VNF_ASSERT(key_len);
  VNF_ASSERT(column);
  VNF_ASSERT(column->record);

  if (!check_db_handle_valid(db_handle)) {
    return DB_HANDLE_INVALID;
  }

  //int dbtbl_type = DBTBL_TYPE_ID(db_handle->id);
  //int dbgroup = DBTBL_GROUP_ID(db_handle->id);

  // from the group, find the groupName, 
  // from the table in the group, find the tableTypeName
  // 
  // SADD tableTypeName <key>  // this gives all rows in the tableType
  // HSET tableTypeName.<key>  <col1> <val1> ...
  // DBCdbTBL_name[db_handle->dbtbl_id]
  // TODO 

  hashkey_len = construct_hashkey(db_handle, key, key_len, hashkey, sizeof(hashkey));

  // run as transaction - multi-exec block
  if (start_transaction(db_handle) != DB_SUCCESS) {
    return DB_ERROR;
  }

  // first add the key to the set for the table - the dbtbl_id itself uniquely identifies a set
  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  construct_setkey(db_handle, setkey, sizeof(setkey));
  sprintf(cmd.cmd, "SADD");
  sprintf(cmd.cmdarg, "%s", setkey);
  cmd.key_len = key_len;
  cmd.key = key;
  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    ret = DB_ERROR;
    goto EXIT;
  }
  
  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  sprintf(cmd.cmd, "HSET");
  cmd.key_len = hashkey_len;
  cmd.key = hashkey;

  cmd.argc = 0;
  cmd.argv[cmd.argc] = cmd.cmd;
  cmd.argvlen[cmd.argc++] = strlen(cmd.cmd);
  cmd.argv[cmd.argc] = (const char *)hashkey;
  cmd.argvlen[cmd.argc++] = hashkey_len;

  // field1 val1
  memcpy(val, &column->record_type, sizeof(column->record_type));
  vallen = sizeof(column->record_type);
  RecordMetaInfo * rm = lookup_record_meta_table(column->record_type);
  VNF_ASSERT(rm);
  vallen += rm->pack(column->record, (uint8_t*)(val+vallen));
  VNF_ASSERT(vallen < DB_CMD_DATA_MAX_LEN *3/4); // ?? PK FIX ME

  cmd.argv[cmd.argc] = (const char *) &column->column_id;
  cmd.argvlen[cmd.argc++] = sizeof(column->column_id);
  cmd.argv[cmd.argc] = (const char *)val;
  cmd.argvlen[cmd.argc++] = vallen;

  // the command is 
  // redis>hset hashkey field1 val1 
  // redis sync API call 
  //
  // redisCommand(context, "hset %b %b %b", hashkey, hashkey_len, field1, field1_len, val1, val1_len
  // or redisCommand(context, "%s %b %b %b", "hset", hashkey, hashkey_len, field1, field1_len, val1, val1_len
  // or redisCommandArgv(context, 4, argv, argvlen)
  //
  // where argv[0]    = "hset"
  //       argvlen[0] = strlen(argv[0])+1;
  //       argv[1]    = hashkey
  //       argvlen[1] = hashkey_len
  //       argv[2]    = field1
  //       argvlen[2] = field1_len
  //       argv[3]    = val1
  //       argvlen[3] = val1_len
  //

  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    ret = DB_ERROR;
    goto EXIT;
  }

  if (exec_transaction(db_handle, false) != DB_SUCCESS) {
    ret = DB_ERROR;
    goto EXIT;
  }

EXIT:
  if (db_handle->start_transaction)
    discard_transaction(db_handle);
  return ret;
}

db_error_t db_write_single_row_sync(db_handle_sync_t *db_handle,
                                    uint8_t* key, uint32_t key_len,
                                    db_column_t *columns, size_t num_columns)
{
  // get rid of gcc warning
  if (num_columns < 1) {
    return (write_single_row_hmset(db_handle, key, key_len, columns, num_columns));
  } else {
    return (write_single_row_hset(db_handle, key, key_len, columns, num_columns));
  }
}

db_error_t db_delete_single_row_sync(db_handle_sync_t *db_handle,
                                     uint8_t* key, uint32_t key_len) 
{
  db_sync_cmd_data_t cmd;
  uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
  int hashkey_len = 0;
  db_error_t ret = DB_SUCCESS;
  uint8_t setkey[128];
 
  VNF_ASSERT(key);
  VNF_ASSERT(key_len);

  if (!check_db_handle_valid(db_handle)) {
    return DB_HANDLE_INVALID;
  }

  // run as transaction
  if (start_transaction(db_handle) != DB_SUCCESS) {
    return DB_ERROR;
  }

  // remove the key from the set for the table - the dbtbl_id itself uniquely identifies a set
  construct_setkey(db_handle, setkey, sizeof(setkey));

  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  sprintf(cmd.cmd, "SREM");
  sprintf(cmd.cmdarg, "%s", setkey);

  cmd.key_len = key_len;
  cmd.key = key;
  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    ret = DB_ERROR;
    goto EXIT;
  }

  // DEL the hashkey representing the row
  hashkey_len = construct_hashkey(db_handle, key, key_len, hashkey, sizeof(hashkey));

  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  sprintf(cmd.cmd, "DEL");
  cmd.key_len = hashkey_len;
  cmd.key = hashkey;

  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    ret = DB_ERROR;
    goto EXIT;
  }

  if (exec_transaction(db_handle, false) != DB_SUCCESS) {
    ret = DB_ERROR;
    goto EXIT;
  }

EXIT:

  if (db_handle->start_transaction)
    discard_transaction(db_handle);

  return ret;
}

db_error_t db_delete_all_row_sync(db_handle_sync_t *db_handle)
{
  // find all keys in the dbtbl_id set and delete hashkey for each
  //
  // DO SSCAN of the set dbtbl_id and del the hashkey 
  // when done with hashkeys, del the set dbtbl_id

  // FIXME - PK Use SMEMBERS for now
  db_sync_cmd_data_t cmd;
  db_sync_cmd_data_t cmd2;
  uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
  int hashkey_len = 0;
  db_error_t ret = DB_SUCCESS;
  int i;
  uint8_t setkey[128];
 
  if (!check_db_handle_valid(db_handle)) {
    return DB_HANDLE_INVALID;
  }

  construct_setkey(db_handle, setkey, sizeof(setkey));

  // remove the key from the set for the table - the dbtbl_id itself uniquely identifies a set
  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  sprintf(cmd.cmd, "SMEMBERS");
  sprintf(cmd.cmdarg, "%s", setkey);
  cmd.process_reply = true;
  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    VNF_ASSERT(!cmd.reply);
    return DB_ERROR;
  }

  VNF_ASSERT(cmd.reply);

  // DEL all hashkeys representing a row
  for (i=0; i<cmd.reply->elements; i++)
  {
    redisReply *rp = cmd.reply->element[i];

    if (rp && rp->len > 0 && rp->type == REDIS_REPLY_STRING)
    {
      hashkey_len = construct_hashkey(db_handle, (uint8_t*)rp->str, rp->len, hashkey, sizeof(hashkey));
      memset(&cmd2, 0, sizeof(db_sync_cmd_data_t));
      sprintf(cmd2.cmd, "DEL");
      cmd2.key_len = hashkey_len;
      cmd2.key = hashkey;

      if (exec_sync_cmd(db_handle, &cmd2, true) != DB_SUCCESS) {
        ret = DB_ERROR;
        goto EXIT;
      }
    }
  }

  // Now del the set for dbtbl_id
  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  sprintf(cmd.cmd, "DEL");
  sprintf(cmd.cmdarg, "%s", setkey);
  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    ret = DB_ERROR;
    goto EXIT;
  }

EXIT:
  if (cmd.reply) freeReplyObject(cmd.reply);
  return ret;
}

db_error_t db_read_single_row_columns_sync(db_handle_sync_t *db_handle,
                                           uint8_t* key, uint32_t key_len,
                                           db_column_t *columns, size_t num_columns)
{
  db_sync_cmd_data_t cmd;
  uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
  int hashkey_len = 0;
  db_error_t ret = DB_SUCCESS;
  db_column_t *column;
  int i;
 
  if (!check_db_handle_valid(db_handle)) {
    return DB_HANDLE_INVALID;
  }

  // construct hashkey, do HMGET for all columns specified
  hashkey_len = construct_hashkey(db_handle, key, key_len, hashkey, sizeof(hashkey));
  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  sprintf(cmd.cmd, "HMGET");

  cmd.argc = 0;
  cmd.argv[cmd.argc] = cmd.cmd;
  cmd.argvlen[cmd.argc++] = strlen(cmd.cmd);
  cmd.argv[cmd.argc] = (const char *)hashkey;
  cmd.argvlen[cmd.argc++] = hashkey_len;

  // HMGET HASHKEY FIELD1...FIELDN 
  VNF_ASSERT((num_columns < (sizeof(cmd.argv)/sizeof(cmd.argv[0])-2)));

  for (i=0; i < num_columns; i++) {
    // field[i] 
    cmd.argv[cmd.argc] = (const char *) &columns[i].column_id;
    cmd.argvlen[cmd.argc++] = sizeof(columns[i].column_id);
    //VNF_ASSERT(!columns[i].record);
  }

  cmd.process_reply = true;

  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
      if(cmd.reply) {
          printf("%s %s\n", __FUNCTION__, cmd.reply->str);
      }
      ret = DB_ERROR;
      goto EXIT;
  }

  VNF_ASSERT(cmd.reply);

  // move the data to column records
  VNF_ASSERT(cmd.reply->elements == num_columns);
  for (i=0; i<cmd.reply->elements; i++) {
    redisReply *rp = cmd.reply->element[i];
    VNF_ASSERT(rp);
    column = &columns[i];

    if (rp && rp->len > 0 && rp->type == REDIS_REPLY_STRING)
    {
      VNF_ASSERT(rp->len > sizeof(int));
      //VNF_ASSERT(column->record_type == *((int*)(rp->str)));
      column->record_type = *((int*)(rp->str));

      RecordMetaInfo * rm = lookup_record_meta_table(column->record_type);
      VNF_ASSERT(rm);

      int record_len = rp->len-sizeof(column->record_type);
      uint8_t *record = (uint8_t *)rp->str + sizeof(column->record_type);

      column->record = (ProtobufCMessage*)(rm->unpack(NULL, record_len, record));
      VNF_ASSERT(column->record);
    }

    if (rp && !rp->len) {
       printf("Column %d does not exist in tbl %d\n", column->column_id, db_handle->id);
    }
  }

EXIT:
  if (cmd.reply) freeReplyObject(cmd.reply);

  return ret;
}

db_error_t db_free_read_sync(db_handle_sync_t *db_handle,
                             uint8_t* key, uint32_t key_len,
                             db_column_t *columns, size_t num_columns)
{
  int i;

  for (i = 0; i<num_columns; i++)
  {
    db_column_t *column = &columns[i];
    RecordMetaInfo * rm = lookup_record_meta_table(column->record_type);

    // debug ASSERT - FIX ME - PK
    VNF_ASSERT(rm);
    if(column->record) {
        rm->free_unpacked(column->record, NULL);
    }
  }

  return (DB_SUCCESS);
}

//db_error_t db_read_single_row_sync(db_handle_sync_t *db_handle,
//                                   uint8_t* key, uint32_t key_len,
//                                   db_column_t **columns, size_t *num_columns)
db_error_t db_read_single_row_sync(db_handle_sync_t *db_handle,
                                   uint8_t* key, uint32_t key_len,
                                   db_column_t *columns, size_t num_columns)
{
  db_sync_cmd_data_t cmd;
  uint8_t hashkey[DB_CMD_MAX_KEY_LEN+DB_CMD_HASHKEY_TAG_LEN];
  int hashkey_len = 0;
  db_error_t ret = DB_SUCCESS;
  db_column_t *column;
  int i;
 
  if (!check_db_handle_valid(db_handle)) {
    return DB_HANDLE_INVALID;
  }

  // construct hashkey, do HMGET for all columns specified
  hashkey_len = construct_hashkey(db_handle, key, key_len, hashkey, sizeof(hashkey));
  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  sprintf(cmd.cmd, "HGETALL");

  cmd.argc = 0;
  cmd.argv[cmd.argc] = cmd.cmd;
  cmd.argvlen[cmd.argc++] = strlen(cmd.cmd);
  cmd.argv[cmd.argc] = (const char *)hashkey;
  cmd.argvlen[cmd.argc++] = hashkey_len;

  // HGETALL HASHKEY FIELD1...FIELDN 
  cmd.process_reply = true;

  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    VNF_ASSERT(!cmd.reply);
    ret = DB_ERROR;
    goto EXIT;
  }

  VNF_ASSERT(cmd.reply);

  // move the data to column records
  if (cmd.reply->elements) {
    // let the caller allocate columns
    //*num_columns = cmd.reply->elements;
    //*columns =  g_malloc0(cmd.reply->elements*sizeof(db_column_t));
    //VNF_ASSERT(*columns);
    if (cmd.reply->elements > (2*num_columns)) {
      printf("db_read_single_row_sync failed, number of fields in the row %"PRIdPTR" exceeds number of columns arg %"PRIdPTR"\n", cmd.reply->elements/2, num_columns);
      ret = DB_ERROR;
      goto EXIT;
    }
  }

  redisReply *rp = cmd.reply;

  VNF_ASSERT((rp && rp->type == REDIS_REPLY_ARRAY));

  for (i=0; i<rp->elements; i+=2) {
    column = &columns[i/2];

    redisReply *fieldReply, *valueReply;

    // first entry field and second entry value
    fieldReply = rp->element[i+0]; 
    valueReply = rp->element[i+1]; 

    VNF_ASSERT(fieldReply);
    VNF_ASSERT(valueReply);
    VNF_ASSERT(fieldReply->type == REDIS_REPLY_STRING);
    VNF_ASSERT(valueReply->type == REDIS_REPLY_STRING);

    memcpy(&column->column_id, fieldReply->str, fieldReply->len);

    VNF_ASSERT(valueReply->len > sizeof(int));
    column->record_type = *((int*)(valueReply->str));

    RecordMetaInfo * rm = lookup_record_meta_table(column->record_type);
    VNF_ASSERT(rm);

    int record_len = valueReply->len-sizeof(column->record_type);
    uint8_t *record = (uint8_t *)valueReply->str + sizeof(column->record_type);

    column->record = (ProtobufCMessage*)(rm->unpack(NULL, record_len, record));
    VNF_ASSERT(column->record);
  }

EXIT:
  if (cmd.reply) freeReplyObject(cmd.reply);

  return ret;
}

db_error_t db_get_next_row_sync(db_handle_sync_t *db_handle, uint8_t* key, uint32_t key_len, uint32_t *out_key_len)
{
  db_sync_cmd_data_t cmd;
  uint64_t cursor = 0;
  uint8_t setkey[128];
  uint32_t setkey_len;
  char *countcmd = "COUNT";
  char countstr[32];
  char cursorstr[32];
  uint8_t *pkey; 
  uint32_t len;
  int i;
  int just_started = 0;

  if (!check_db_handle_valid(db_handle)) {
    return DB_HANDLE_INVALID;
  }

  VNF_ASSERT(key);
  VNF_ASSERT(key_len);
  VNF_ASSERT(out_key_len);

  *out_key_len = 0;

  if (db_handle->scan_in_progress) {
    cursor = db_handle->scan_cursor;
  } else {
    db_handle->scan_in_progress = true;
    free_scan_replies(db_handle);
    just_started = 1;
  } 

  uint32_t pi = db_handle->scan_replies_pi;
  uint32_t ci = db_handle->scan_replies_ci;

  // there is an entry from previous SSCAN command
  if (pi != ci) {
    pkey = db_handle->scan_reply_keys[ci];
    len = db_handle->scan_reply_keylen[ci];
    VNF_ASSERT(pkey);
    VNF_ASSERT(len);
    VNF_ASSERT(len <= key_len);

    memcpy(key, pkey, len);
    *out_key_len = len;

    g_free(db_handle->scan_reply_keys[ci]);
    db_handle->scan_reply_keys[ci] = 0; 
    db_handle->scan_reply_keylen[ci] = 0; 

    ci++;
    if (ci >= DB_SYNC_MAX_SCAN_COUNT) {
      ci = 0;
    } 
    db_handle->scan_replies_ci = ci; 
    return DB_SUCCESS;
  }

  if (!just_started && !cursor) {
    // We are really completed here.
    *out_key_len = 0;
    db_handle->scan_in_progress = false;
    return DB_SUCCESS;
  }

  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  setkey_len = construct_setkey(db_handle, setkey, sizeof(setkey));
  sprintf(cmd.cmd, "SSCAN");
  cmd.argc = 0;
  cmd.argv[cmd.argc] = cmd.cmd;
  cmd.argvlen[cmd.argc++] = strlen(cmd.cmd);

  cmd.argv[cmd.argc] = (const char *)setkey;
  cmd.argvlen[cmd.argc++] = setkey_len;

  snprintf(cursorstr, sizeof(cursorstr), "%"PRIu64, cursor);
  cmd.argv[cmd.argc] = (const char *)cursorstr;
  cmd.argvlen[cmd.argc++] = strlen(cursorstr);

  cmd.argv[cmd.argc] = countcmd;
  cmd.argvlen[cmd.argc++] = strlen(countcmd);

  snprintf(countstr, sizeof(countstr), "%d", 1);
  cmd.argv[cmd.argc] = (const char *)countstr;
  cmd.argvlen[cmd.argc++] = strlen(countstr);

  cmd.process_reply = true;
  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    VNF_ASSERT(!cmd.reply);
    return DB_ERROR;
  }

  VNF_ASSERT(cmd.reply);
  VNF_ASSERT(cmd.reply->elements);

  // get the cursor value
  redisReply *rp = cmd.reply->element[0];
  VNF_ASSERT(rp && rp->len > 0 && rp->type == REDIS_REPLY_STRING);
  sscanf(rp->str, "%"PRIu64, &cursor);
  db_handle->scan_cursor = cursor;

  VNF_ASSERT(cmd.reply->elements >= 2);
  rp = cmd.reply->element[1];
  VNF_ASSERT(rp && rp->type == REDIS_REPLY_ARRAY);

  if (!cursor && !rp->elements)
  {
    // We are really completed here too.
    *out_key_len = 0;
    db_handle->scan_in_progress = false;
    goto EXIT;
  }

  VNF_ASSERT(rp->elements);
  redisReply *s = rp->element[0]; 

  VNF_ASSERT(s);
  VNF_ASSERT(s->type == REDIS_REPLY_STRING);
  VNF_ASSERT(s->len <= key_len);
  VNF_ASSERT(s->len);
  memcpy(key, s->str, s->len);
  *out_key_len = s->len;

  printf("SSCAN reply : cursor %"PRId64" element count %"PRIdPTR"\n", cursor, rp->elements);
  //print_data(s->str, s->len);

  VNF_ASSERT((rp->elements-1) <= DB_SYNC_MAX_SCAN_COUNT); 

  int queued_count = 0;
  int free_count = 0;
  if (pi >= ci ) {
    queued_count = pi-ci;
  } else if (pi < ci) {
    queued_count = ci - pi + DB_SYNC_MAX_SCAN_COUNT-1;
  }

  free_count = DB_SYNC_MAX_SCAN_COUNT - queued_count;

  VNF_ASSERT((rp->elements-1) <= free_count); // queue full 

  for (i=1; i < rp->elements; i++) { 
    s = rp->element[i]; 

    VNF_ASSERT(s);
    VNF_ASSERT(s->type == REDIS_REPLY_STRING);
    VNF_ASSERT(s->len <= key_len);
    VNF_ASSERT(s->len);
   
    uint8_t *pkey = g_malloc0(s->len); 
    VNF_ASSERT(pkey);
    memcpy(pkey, s->str, s->len);
    //print_data(s->str, s->len);
    db_handle->scan_reply_keys[pi] = pkey;
    db_handle->scan_reply_keylen[pi] = s->len;
    pi++;
    if (pi >= DB_SYNC_MAX_SCAN_COUNT) {
      pi = 0;
    } 
    db_handle->scan_replies_pi = pi; 
  }

EXIT:
  if (cmd.reply) freeReplyObject(cmd.reply);

  return DB_SUCCESS;
}

/*--------------------------------------------------
 *  Private/local methods start here
 *--------------------------------------------------*/
static db_error_t connect_redis(db_handle_sync_t *db_handle) {
  redisContext *ctx = redisConnect(db_handle->params.server_ip, db_handle->params.server_port);
  if (ctx == NULL) {
    printf("Redis Connect Error: NULL context");
    return DB_CONN_FAILED;
  }

  if (ctx->err) {
    printf("Redis Connect Error: %s\n", ctx->errstr);
    redisFree(ctx);
    return DB_CONN_FAILED;
  }

  db_handle->redis_ctx = ctx;

  VNF_ASSERT(db_handle->redis_ctx);

  printf("Redis Connect success: dbtbl_id %d\n", db_handle->id);
  return DB_SUCCESS;
}

static db_error_t disconnect_redis(db_handle_sync_t *db_handle) {
  if (db_handle->redis_ctx) {
    redisFree(db_handle->redis_ctx);
    db_handle->redis_ctx = NULL;
  }

  return DB_SUCCESS;
}

static db_error_t reconnect_redis(db_handle_sync_t *db_handle) {
  printf("Redis Connect Retrying: dbtbl_id %d\n", db_handle->id);
  disconnect_redis(db_handle);
  return connect_redis(db_handle); 
}

static db_error_t exec_sync_cmd(db_handle_sync_t *db_handle, db_sync_cmd_data_t *cmd, bool retry)
{
  redisReply *reply;
  db_error_t err = DB_SUCCESS;
  int i;

  VNF_ASSERT(db_handle);
  VNF_ASSERT(cmd);

  if (db_handle->redis_ctx == NULL) {
    // we expect REDIS connection to be ready
    if (reconnect_redis((db_handle_sync_t *)db_handle) != DB_SUCCESS) {
      return DB_CONN_FAILED;
    }
  }

  if (cmd->argc) {
    for (i=0; i < cmd->argc; i++) {
      VNF_ASSERT(cmd->argv[i]);
      VNF_ASSERT(cmd->argvlen[i]);
    }
    reply = redisCommandArgv(db_handle->redis_ctx, cmd->argc, cmd->argv, cmd->argvlen);
  } else if (cmd->key_len) {
    VNF_ASSERT(cmd->key);
    if (strlen(cmd->cmdarg)) {
      reply = redisCommand(db_handle->redis_ctx, "%s %s %b", cmd->cmd, cmd->cmdarg, cmd->key, cmd->key_len);
    } else {
      reply = redisCommand(db_handle->redis_ctx, "%s %b", cmd->cmd, cmd->key, cmd->key_len);
    }
  } else { 
    if (strlen(cmd->cmdarg)) 
        reply = redisCommand(db_handle->redis_ctx, "%s %s", cmd->cmd, cmd->cmdarg); 
    else 
        reply = redisCommand(db_handle->redis_ctx, cmd->cmd);
  }

  if (reply == NULL) {
    if (db_handle->redis_ctx->err) {
        printf("Redis Command Error: %s\n", db_handle->redis_ctx->errstr);
    }

    if (retry) {
      if (reconnect_redis((db_handle_sync_t *)db_handle) == DB_SUCCESS) {
        printf("Redis Command Retrying: %s\n", cmd->cmd);
        return (exec_sync_cmd(db_handle, cmd, false)); // retry only once
      }
    }

    printf("Redis Command failed: command %s\n", cmd->cmd);
    err = DB_ERROR;
  } else {
    if (reply->type == REDIS_REPLY_ERROR) {
      printf("Redis Command REPLY_ERROR: error %s command %s\n", reply->str, cmd->cmd);
      err = DB_ERROR;
    } else  if (reply->type == REDIS_REPLY_ARRAY) {
      printf("Redis Command REDIS_REPLY_ARRAY: element count %zu\n", reply->elements);
      for (i=0; i < reply->elements; i++) {
        VNF_ASSERT(!(reply->element[i]->type == REDIS_REPLY_ERROR));
      }
    } 
    else {
      printf("Redis Command success: command %s\n", cmd->cmd);
    }
    if (!cmd->process_reply) {
      freeReplyObject(reply);
    } else {
      cmd->reply = reply;
      cmd->process_reply = false;
    }
  } 

  return (err);
}

static db_error_t append_sync_cmd(db_handle_sync_t *db_handle, db_sync_cmd_data_t *cmd)  
{
  db_error_t err = DB_SUCCESS;
  int i;

  VNF_ASSERT(db_handle);

  if (db_handle->redis_ctx == NULL) {
    // we expect REDIS connection to be ready
    if (reconnect_redis((db_handle_sync_t *)db_handle) != DB_SUCCESS) {
      return DB_CONN_FAILED;
    }
  }

  if (cmd->argc) {
    for (i=0; i < cmd->argc; i++) {
      VNF_ASSERT(cmd->argv[i]);
      VNF_ASSERT(cmd->argvlen[i]);
    }
    redisAppendCommandArgv(db_handle->redis_ctx, cmd->argc, cmd->argv, cmd->argvlen);
  } else if (cmd->key_len) {
    VNF_ASSERT(cmd->key);
    redisAppendCommand(db_handle->redis_ctx, "%s %b", cmd->cmd, cmd->key, cmd->key_len);
  } else {
    redisAppendCommand(db_handle->redis_ctx, cmd->cmd);
  }

  return (err);
}

static db_error_t get_reply_sync_cmd(db_handle_sync_t *db_handle, db_sync_get_reply_t *get_reply, bool retry)  
{
  db_error_t err = DB_SUCCESS;
  redisReply *reply = NULL;

  VNF_ASSERT(db_handle);
  VNF_ASSERT(get_reply);

  // debug ASSERT ? FIXME PK
  VNF_ASSERT(redisGetReply(db_handle->redis_ctx, (void **)&reply) == REDIS_OK);

  if (reply == NULL) {
    if (db_handle->redis_ctx->err) {
      printf("Redis Command Error: %s\n", db_handle->redis_ctx->errstr);
    }
    // retry does not make sense unless connection error - TODO - FIX ME - PK
    retry = false;
    if (retry && get_reply->cmd) {
      if (reconnect_redis((db_handle_sync_t *)db_handle) == DB_SUCCESS) {
        printf("Redis Command Retrying: %s\n", get_reply->cmd->cmd);
        return (exec_sync_cmd(db_handle, get_reply->cmd, false)); // retry only once
      }
    }
    printf("Redis Command failed: command %s\n", get_reply->cmd->cmd);
    err = DB_ERROR;
  } else {
    if (reply->type == REDIS_REPLY_ERROR) {
      printf("Redis Command REPLY_ERROR: error %s command %s\n", reply->str, get_reply->cmd->cmd);
    } else {
#ifdef DEBUG_DB
      printf("Redis Command success: command %s\n", get_reply->cmd->cmd);
#endif
    }
    freeReplyObject(reply);
  }

  return err;
}

static bool check_db_handle_valid(db_handle_sync_t *db_handle) {
  if ((db_handle == NULL) || (db_handle->magic != DB_HANDLE_SYNC_MAGIC)) {
    printf("DB Invalid Handle : 0x%016" PRIXPTR, (uintptr_t)db_handle);
    if(db_handle) {
        printf("DB Invalid Handle: magic 0x%x\n", db_handle->magic);
    }
    return false;
  }

  return true;
}

static bool check_dbtbl_id_valid(db_id_t dbtbl_id) {
  return true;
}

uint32_t construct_hashkey(db_handle_sync_t *db_handle, uint8_t *key, uint32_t key_len, uint8_t *hashkey, uint32_t hashkey_len) 
{
  return (construct_hashkey_common(db_handle->id, key, key_len, hashkey, hashkey_len));
}

uint32_t construct_setkey(db_handle_sync_t *db_handle, uint8_t *setkey, uint32_t setkey_len) 
{
  return (construct_setkey_common(db_handle->id, setkey, setkey_len));
}

db_error_t write_single_row_hset(db_handle_sync_t *db_handle,
                                 uint8_t* key, uint32_t key_len,
                                 db_column_t *columns, size_t num_columns)
{
  db_error_t ret = DB_SUCCESS;  

  db_sync_cmd_data_t *cmds = NULL;     
  db_sync_cmd_data_t *cmd = NULL;     
  db_sync_cmd_data_t cmd1; 

  db_sync_get_reply_t *get_replies = NULL;     
  db_sync_get_reply_t *get_reply = NULL;     

  redisReply **replies = NULL;
  //redisReply *reply = NULL;

  uint8_t hashkey[DB_CMD_MAX_KEY_LEN];
  int hashkey_len = 0;
  uint8_t val[DB_CMD_DATA_MAX_LEN];
  int vallen = 0;
  db_column_t *column;
  uint8_t setkey[128];
  int i;

  VNF_ASSERT(key != NULL);
  VNF_ASSERT(key_len != 0);
  VNF_ASSERT(columns != NULL);

  if (!check_db_handle_valid(db_handle)) {
    return DB_HANDLE_INVALID;
  }

  // allocate memory for cmds
  cmds = g_malloc0(num_columns*sizeof(db_sync_cmd_data_t));
  replies = g_malloc0(num_columns*sizeof(redisReply *));
  get_replies = g_malloc0(num_columns*sizeof(db_sync_get_reply_t));

  // debug ASSERT
  VNF_ASSERT(cmds);
  VNF_ASSERT(replies);
  VNF_ASSERT(get_replies);

  if ((!cmds) || (!replies) || (!get_replies)) {
    printf("Memory allocation failure: File %s Function %s Line %d\n", __FILE__, __FUNCTION__, __LINE__ );
    ret = DB_ERROR;
    goto EXIT;
  }

  hashkey_len = construct_hashkey(db_handle, key, key_len, hashkey, sizeof(hashkey));

  // run as transaction - multi-exec block
  if (start_transaction(db_handle) != DB_SUCCESS) {
    printf("REDIS start transaction command failed: File %s Function %s Line %d\n", __FILE__, __FUNCTION__, __LINE__ );
    ret = DB_ERROR;
    goto EXIT;
  }
  
  // first add the key to the set for the table - the dbtbl_id itself uniquely identifies a set
  memset(&cmd1, 0, sizeof(db_sync_cmd_data_t));
  construct_setkey(db_handle, setkey, sizeof(setkey));
  sprintf(cmd1.cmd, "SADD");
  sprintf(cmd1.cmdarg, "%s", setkey);
  cmd1.key_len = key_len;
  cmd1.key = key;
  if (exec_sync_cmd(db_handle, &cmd1, true) != DB_SUCCESS) {
    ret = DB_ERROR;
    goto EXIT;
  }
  
  // Use HMSET instead of HSET ? FIXME - PK
  for (i=0; i < num_columns; i++) {
    column = &columns[i];
    cmd = &cmds[i];
    get_replies[i].cmd = cmd;
    get_replies[i].reply = replies[i];

    memset(cmd, 0, sizeof(db_sync_cmd_data_t));
    sprintf(cmd->cmd, "HSET");

    cmd->argc = 0;
    cmd->argv[cmd->argc] = cmd->cmd;
    cmd->argvlen[cmd->argc++] = strlen(cmd->cmd);
    cmd->argv[cmd->argc] = (const char *)hashkey;
    cmd->argvlen[cmd->argc++] = hashkey_len;

    // field[i] val[i]
    memcpy(val, &column->record_type, sizeof(column->record_type));
    vallen = sizeof(column->record_type);
    RecordMetaInfo * rm = lookup_record_meta_table(column->record_type);
    VNF_ASSERT(rm);
    VNF_ASSERT(column->record);
    vallen += rm->pack(column->record, (uint8_t*)(val+vallen));
    VNF_ASSERT(vallen < DB_CMD_DATA_MAX_LEN *3/4); // ?? PK FIX ME

    cmd->argv[cmd->argc] = (const char *) &column->column_id;
    cmd->argvlen[cmd->argc++] = sizeof(column->column_id);
    cmd->argv[cmd->argc] = (const char *)val;
    cmd->argvlen[cmd->argc++] = vallen;

    if (append_sync_cmd(db_handle, cmd) != DB_SUCCESS) {
      ret = DB_ERROR;
      goto EXIT;
    }
  }

  for (i=0; i < num_columns; i++) {
    get_reply = &get_replies[i];
    if (get_reply_sync_cmd(db_handle, get_reply, false) != DB_SUCCESS) {
      ret = DB_ERROR;
      goto EXIT;
    }
  }

  if (exec_transaction(db_handle, false) != DB_SUCCESS) {
      ret = DB_ERROR;
      goto EXIT;
  }

EXIT:
  if (cmds) g_free(cmds);
  if (replies) g_free(replies);
  if (get_replies) g_free(get_replies);

  if (db_handle->start_transaction)
    discard_transaction(db_handle); 
   
  return ret; 
}

db_error_t write_single_row_hmset(db_handle_sync_t *db_handle,
                                  uint8_t* key, uint32_t key_len,
                                  db_column_t *columns, size_t num_columns)
{
  db_sync_cmd_data_t cmd;
  uint8_t hashkey[DB_CMD_MAX_KEY_LEN];
  int hashkey_len = 0;
  db_error_t ret = DB_SUCCESS;
  db_column_t *column;
  uint8_t val[DB_CMD_DATA_MAX_LEN];
  int vallen = 0;
  int i;
  uint8_t setkey[128];
 
  if (!check_db_handle_valid(db_handle)) {
    return DB_HANDLE_INVALID;
  }

  // construct hashkey, do HMGET for all columns specified
  hashkey_len = construct_hashkey(db_handle, key, key_len, hashkey, sizeof(hashkey));

  // run as transaction - multi-exec block
  if (start_transaction(db_handle) != DB_SUCCESS) {
    return DB_ERROR;
  }

  // first add the key to the set for the table - the dbtbl_id itself uniquely identifies a set
  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  construct_setkey(db_handle, setkey, sizeof(setkey));
  sprintf(cmd.cmd, "SADD");
  sprintf(cmd.cmdarg, "%s", setkey);
  cmd.key_len = key_len;
  cmd.key = key;
  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    return DB_ERROR;
  }
  
  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  sprintf(cmd.cmd, "HMSET");

  cmd.argc = 0;
  cmd.argv[cmd.argc] = cmd.cmd;
  cmd.argvlen[cmd.argc++] = strlen(cmd.cmd)+1;
  cmd.argv[cmd.argc] = (const char *)hashkey;
  cmd.argvlen[cmd.argc++] = hashkey_len;

  // HMSET HASHKEY FIELD1 VAL1 ... FIELDN VALN
  VNF_ASSERT((2*num_columns < (sizeof(cmd.argv)/sizeof(cmd.argv[0])-2)));

  for (i=0; i < num_columns; i++) {
    column = &columns[i];
    // field[i] val[i]
    memcpy(val, &column->record_type, sizeof(column->record_type));
    vallen = sizeof(column->record_type);
    RecordMetaInfo * rm = lookup_record_meta_table(column->record_type);
    VNF_ASSERT(rm);
    VNF_ASSERT(column->record);
    vallen += rm->pack(column->record, (uint8_t*)(val+vallen));
    VNF_ASSERT(vallen < DB_CMD_DATA_MAX_LEN *3/4); // ?? PK FIX ME

    cmd.argv[cmd.argc] = (const char *) &column->column_id;
    cmd.argvlen[cmd.argc++] = sizeof(column->column_id);
    cmd.argv[cmd.argc] = (const char *)val;
    cmd.argvlen[cmd.argc++] = vallen;
  }

  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    ret = DB_ERROR;
    goto EXIT;
  }

  if (exec_transaction(db_handle, true) != DB_SUCCESS) {
      ret = DB_ERROR;
      goto EXIT;
  }
  VNF_ASSERT(cmd.reply);

  // the commands were SADD and HMSET 
  VNF_ASSERT(cmd.reply->elements == 2);
  redisReply *rp = cmd.reply->element[0];
  VNF_ASSERT(rp);
  VNF_ASSERT(rp->type == REDIS_REPLY_INTEGER);
  printf("SADD row %lld to table %d\n", rp->integer, db_handle->id);

  rp = cmd.reply->element[1];
  VNF_ASSERT(rp);
  VNF_ASSERT(rp->type == REDIS_REPLY_STATUS);
  printf("HMSET set fields of row %lld to table %d status %s\n", rp->integer, db_handle->id, rp->str);

EXIT:
  if (cmd.reply) freeReplyObject(cmd.reply);

  discard_transaction(db_handle);

  return ret;
}

db_error_t start_transaction(db_handle_sync_t *db_handle) 
{
  db_sync_cmd_data_t cmd;
  db_error_t ret = DB_SUCCESS;

  if (db_handle->start_transaction) {
    printf("REDIS start transaction rejected already pending: File %s Function %s Line %d\n", __FILE__, __FUNCTION__, __LINE__ );
    return DB_ERROR;
  }

  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  sprintf(cmd.cmd, "MULTI");
  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    printf("REDIS start transaction command failed: File %s Function %s Line %d\n", __FILE__, __FUNCTION__, __LINE__ );
    ret = DB_ERROR;
    db_handle->start_transaction = false;
  } else {
    db_handle->start_transaction = true;
  }

  return ret;
}

db_error_t exec_transaction(db_handle_sync_t *db_handle, bool process_reply) 
{
  db_sync_cmd_data_t cmd;
  db_error_t ret = DB_SUCCESS;

  if (!db_handle->start_transaction) {
    printf("REDIS exec transaction rejected transaction not pending: File %s Function %s Line %d\n", __FILE__, __FUNCTION__, __LINE__ );
    return DB_ERROR;
  }

  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  cmd.process_reply = process_reply;
  sprintf(cmd.cmd, "EXEC");
  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    printf("REDIS exec transaction command failed: File %s Function %s Line %d\n", __FILE__, __FUNCTION__, __LINE__ );
    ret = DB_ERROR;
  } 

  db_handle->start_transaction = false;
  return ret;
}

db_error_t discard_transaction(db_handle_sync_t *db_handle) 
{
  db_sync_cmd_data_t cmd;
  db_error_t ret = DB_SUCCESS;

  if (!db_handle->start_transaction) {
    printf("REDIS discard transaction rejected transaction not pending: File %s Function %s Line %d\n", __FILE__, __FUNCTION__, __LINE__ );
    return DB_ERROR;
  }

  memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
  sprintf(cmd.cmd, "DISCARD");
  if (exec_sync_cmd(db_handle, &cmd, true) != DB_SUCCESS) {
    printf("REDIS discard transaction command failed: File %s Function %s Line %d\n", __FILE__, __FUNCTION__, __LINE__ );
    ret = DB_ERROR;
  } 

  db_handle->start_transaction = false;
  return ret;
}

void setup_db_params(db_handle_sync_t *db_handle, db_handle_sync_param_t *params) 
{
  setup_default_db_params(db_handle);

  if (params) {
    // check for valid port and server ip address
    if(strlen(params->server_ip)) {
      strcpy(db_handle->params.server_ip, params->server_ip);
    }
    else {
        strcpy(db_handle->params.server_ip, db_local_ip_default);
    }
    if(params->server_port) {
        db_handle->params.server_port = params->server_port;
    } 
    else {
        db_handle->params.server_port = db_local_port_default;
    }
  }
}

void setup_default_db_params(db_handle_sync_t *db_handle) 
{
  memset(&db_handle->params, 0, sizeof(db_handle->params));
  
  // set defaults
  db_handle->params.timeout_ms = DB_SYNC_CMD_TIMEOUT_MS; // 5sec
  db_handle->params.num_keytypes = 1; // one key to read/write/traverse 

  db_handle->params.keytype_column_ids[0] = 1; // ? O FIXME TODO PK
  strcpy((char *)&db_handle->params.server_ip, REDIS_SERVER_DEFAULT_IP);
  db_handle->params.server_port = REDIS_SERVER_DEFAULT_PORT;

  db_handle->params.max_row_count = DB_SYNC_MAX_ROWS ; //  maximum number of rows to expect in this table
  db_handle->params.max_column_size = DB_SYNC_MAX_COLUMN_SIZE; // maximum size of the data in a column
  db_handle->params.max_columns = DB_SYNC_MAX_COLUMNS; // maximum number of columns to expect in this table
}

void free_scan_replies(db_handle_sync_t *db_handle)
{
  int i;

  for (i=0; i < DB_SYNC_MAX_SCAN_COUNT+1; i++) {
    if (db_handle->scan_reply_keys[i]) {
      g_free(db_handle->scan_reply_keys[i]);
      db_handle->scan_reply_keys[i] = 0;
    }
    db_handle->scan_reply_keylen[i] = 0;
  }

  db_handle->scan_replies_pi = db_handle->scan_replies_ci = 0;
}

#if 0
void print_data(uint8_t *buf, uint32_t len)
{
  int i;

  for (i = 0; i < len; i++) {
    printf("0x%02x ", buf[i]);
    if (i%16) {
      printf("\n");
    }
  }
}
#endif

#ifdef NOT_YET
db_error_t db_become_master(db_handle_sync_t *db_handle)
{
    db_error_t ret = DB_SUCCESS;
    db_sync_cmd_data_t cmd;
    memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
    sprintf(cmd.cmd, "SLAVEOF NO ONE");
    ret = exec_sync_cmd(db_handle, &cmd, true);
    if(ret != DB_SUCCESS) {
        printf("REDIS exec transaction command failed: File %s Function %s Line %d, ret %d\n", __FILE__, __FUNCTION__, __LINE__, ret);
        ret = DB_ERROR;
    } 

    return ret;
}

db_error_t db_slave_of(char *ip, db_handle_sync_t *db_handle)
{
    VNF_ASSERT(ip != NULL);
    VNF_ASSERT(strlen(ip) != 0);

    db_error_t ret = DB_SUCCESS;
    db_sync_cmd_data_t cmd;
    memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
    sprintf(cmd.cmd, "SLAVEOF %s 6379", ip);
    ret = exec_sync_cmd(db_handle, &cmd, true);
     if(ret != DB_SUCCESS) {
        printf("REDIS exec transaction command failed: File %s Function %s Line %d, ret %d\n", __FILE__, __FUNCTION__, __LINE__, ret);
        ret = DB_ERROR;
    } 

    return DB_SUCCESS;
}

static void db_publish_row_change_sync(db_handle_sync_t *db_handle,
                             uint8_t* key, size_t key_len,
                             int event,
                             int num_column,
                             int32_t *column_id)
{
    uint8_t buf[512];
    uint8_t ch[32];
    HaChRowMsg msg;
    size_t data_len;
    size_t ch_len; 
    redisReply *reply;


    ha_ch_row_msg__init(&msg);

    msg.has_event = 1;
    msg.event = event;

    msg.has_key = 1;
    msg.key.data = key;
    msg.key.len = key_len;

    switch (event)
    {
        case HA_CH_ROW_MSG__EVENT__DELETE:
        case HA_CH_ROW_MSG__EVENT__COMPLETE_UPDATE:
            msg.n_column_id = 0;
            msg.column_id = NULL;
            break;
        case HA_CH_ROW_MSG__EVENT__PARTIAL_UPDATE:
            msg.n_column_id = num_column;
            msg.column_id = column_id;
            break;
        default:
            return;
    }

    data_len = HaChRowMsgFns->pack(&msg, buf);
    ch_len = sprintf((char *) ch, "%s%u", DB_ROW_CHANGE_NOTIFICATION_CH_TAG, db_handle->id);
    reply = redisCommand(db_handle->redis_ctx, "PUBLISH %b %b", ch, ch_len, buf, data_len);
    if (reply)
    {
        freeReplyObject(reply);
    }
}
#endif

static void db_init_new_status_sync(gpointer data, gpointer user_data)
{
    db_handle_sync_t *db_handle = (db_handle_sync_t *) data;

    if (db_handle->connect_mode == DB_CONNECT_MASTER)
    {
       if (DB_SUCCESS != reconnect_redis(db_handle)) 
           reconnect_redis(db_handle); // retry; need sleep?
    }
}

static void db_new_status_sync()
{
    db_init_internal_sync();
    g_queue_foreach(db_handle_queue_sync, db_init_new_status_sync, 0);
}

extern void db_new_status(int type);

/** The function is called when remote Redis server address becomes available or changed.
    The function will detect if the info is redundant/repeated. */
void db_new_remote_server_ip(char *ip)
{
    printf("%s ip %s\n", __FUNCTION__, ip);

    if (!strcmp(ip, remote_server_ip))
        return;

    strcpy(remote_server_ip, ip);

    if (local_is_master)
        return;

    db_new_status_sync();
    db_new_status(DB_NEW_STATUS_REMOTE_SERVER_IP);
}

/** This function is called when the local node changes status from master to slave. 
    Note that by default, db_api assumes the local node is master. The function will detect 
    if the info is redundant/repeated. */
void db_is_slave(void)
{
    printf("%s\n", __FUNCTION__);

    if (!local_is_master)
        return;

    local_is_master = 0;

    db_new_status_sync();
    db_new_status(DB_NEW_STATUS_SLAVE);
}

/** This function is called when the local node changes status from slave to master. 
    The function will detect if the info is redundant/repeated. */
void db_is_master(void)
{
    printf("%s\n", __FUNCTION__);

    if (local_is_master)
        return;

    local_is_master = 1;

    db_new_status_sync();
    db_new_status(DB_NEW_STATUS_MASTER);
}

db_error_t db_check_slave_initial_replication_sync(db_handle_sync_t *db_handle, int *answer)
{
    db_error_t ret = DB_SUCCESS;
    db_sync_cmd_data_t cmd;
    redisReply *reply;
    char *p;
    char *master_link_status = "master_link_status:";
    char *master_sync_in_progress = "master_sync_in_progress:";
    char *master_last_io_seconds_ago = "master_last_io_seconds_ago:";

    *answer = 0;
    memset(&cmd, 0, sizeof(db_sync_cmd_data_t));
    cmd.process_reply = true;
    sprintf(cmd.cmd, "info replication");
    ret = exec_sync_cmd(db_handle, &cmd, true);
    if(ret != DB_SUCCESS) 
        return DB_ERROR;

    reply = cmd.reply;
    if (reply == NULL || reply->type != REDIS_REPLY_STRING || reply->len <= 0)
    {
        ret = DB_ERROR;
        goto exit;
    }

    reply->str[reply->len] = '\0'; 

    printf("reply=%s\n", reply->str);

    //"master_link_status:up"
    p = strstr(reply->str, master_link_status);
    if (!p) goto exit;

    p += strlen(master_link_status);
    if (strncmp(p, "up", strlen("up"))) goto exit;

    //"master_sync_in_progress:<value>", check if value == 0
    p = strstr(reply->str, master_sync_in_progress);
    if (!p) goto exit;

    p += strlen(master_sync_in_progress);
    if (*p != '0') goto exit;

    //"master_last_io_seconds_ago:<value>", check if value >= 0
    p = strstr(reply->str, master_last_io_seconds_ago);
    if (!p) goto exit;

    p += strlen(master_last_io_seconds_ago);
    if (atoi(p) == 0) goto exit;

    *answer = 1;

    printf("Slave initial db sync is done.\n");

exit:
    if (reply) freeReplyObject(reply);
   
    return ret;
}
