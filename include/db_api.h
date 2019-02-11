#ifndef __DATABASE_API_H__
#define __DATABASE_API_H__

#include "vnf_assert.h"
#include <inttypes.h>
#include "db_group.h"
#include "db_ha.h"

#include <protobuf-c/protobuf-c.h>

const char *db_local_ip_default;
const int   db_local_port_default;
extern int logging_level_db_api;

typedef enum {
    L_CLI    = 0,
    L_SNMP   = 1,
    L_QAM    = 2,
    L_IMM    = 3,
    L_RPC    = 4,
    L_SYS    = 5,
    L_CMTS   = 6,
    L_DOCSIS_OB = 7, /*  obsoleted, docsis-log moved to snmpd */
    L_ALARM  = 8,
    L_DPI    = 9,
    L_DMM    = 10,
    L_RNG    = 11,
    L_MAC    = 12,
    L_BPI    = 13,
    L_DCTS   = 14,
    L_EAM    = 15,
    L_LC     = 16,
    L_IPDR   = 17,
    L_IPSEC  = 18,
    L_ROUTER = 19,
    L_AAA    = 20,
    L_CSM_TRA = 21,
    L_CSM_POL = 22,
    L_CSM_FFT = 23,
    L_CSM_EVT = 24,
    L_CSM_FSM = 25,
    L_CSM_HOP = 26,
    L_CSM_ERR = 27,
    L_CDB = 28,
    L_CFG = 29,
    L_LBM =30,
    L_CMREMOTE = 31,
    L_SIM = 32,
    L_PKTCB = 33,
    L_PME = 34,
    L_USR_MAC  = 35,
    L_PK = 36,
    L_STATIS = 37,
    L_RPM = 38,
    L_DBAPI = 39,
    L_OTHER  = 40,
    L_LTEGW = 41, 
    L_VNFHA = 42,
    L_FFE_CTRL = 43,
    L_FFE_DATA = 44,
    L_FFE_MGR = 45,
    L_WIFI = 46,
    L_DHCPMGR = 47,
    L_DPOE = 48,
    L_OAM = 49,
    L_NUMBERS
} LOG_MODULE;


typedef enum LogLevel {
    L_EMERGENCIES   = 0,
    L_ALERTS        = 1,
    L_CRITICAL      = 2,
    L_ERRORS        = 3,
    L_WARNINGS      = 4,
    L_NOTIFICATIONS = 5,
    L_INFORMATIONAL = 6,
    L_DEBUGGING     = 7
} LOG_LEVEL;


/** The dbapi module provides interface to read/write data
 *  from the underlying database. This API can be used to
 *  store and retrieve configuration of the VNF; session
 *  state information for intra and inter VNF recovery */

#define DB_ERROR_LIST(DB_ERROR_DEF)   \
    DB_ERROR_DEF(DB_SUCCESS, "DB_Success")               \
    DB_ERROR_DEF(DB_PENDING, "DB Connection Pending")    \
    DB_ERROR_DEF(DB_RETRY, "DB Retry cmd later")         \
    DB_ERROR_DEF(DB_HANDLE_INVALID, "DB Handle Invalid") \
    DB_ERROR_DEF(DB_CONN_FAILED, "DB Conn Failed")       \
    DB_ERROR_DEF(DB_CONN_TIMEOUT, "DB Conn Timeout")     \
    DB_ERROR_DEF(DB_ERROR_NOT_EXIST, "DB element not exist") \
    DB_ERROR_DEF(DB_ERROR, "DB Error")                   \
    DB_ERROR_DEF(DB_ERROR_FILE_OP, "DB file operation error") \
    DB_ERROR_DEF(DB_ERROR_FULL_READ_IN_PROGRESS, "DB full read is in progress") \
    DB_ERROR_DEF(DB_ERROR_DELETE_ALL_IN_PROGRESS, "DB delete all rows is in progress") \
    DB_ERROR_DEF(DB_DELETE_ALL_ROW_COMPLETE, "DB_Delete_All_Row_Complete") \
    DB_ERROR_DEF(DB_PARTIAL_WRITE, "DB partial write first") \
    DB_ERROR_DEF(DB_WRONG_CONNECT_MODE, "DB handle with wrong connect mode") \
    DB_ERROR_DEF(DB_ERROR_INVALID, "DB Invalid Error")  

typedef enum {
#define  DB_ERROR_DEF(X, Y) X,
    DB_ERROR_LIST(DB_ERROR_DEF)
#undef DB_ERROR_DEF
} db_error_t;


typedef int32_t db_id_t;   

const char *db_error_str(db_error_t err);

/** db_column_t, contains information belonging to one single column  */
typedef struct {
    int32_t           column_id; //column_name  = DBHaSaColumn_name[column_id];
    int32_t           record_type;
    ProtobufCMessage* record;
} db_column_t;

/** Called from DB API to user module when write operation finishes
 *
 *  @param   err the return code for the write operation
 *  @param   cb_arg the callback argument passed in when opening db_handle
 *  @param   key the key used when calling db_write(). A copy of key passed in when calling db_write().
 *  @param   key_len the key length when calling db_write() */
typedef void (*db_write_callback_func_t) (db_error_t err,
                                          void *cb_arg, 
                                          uint8_t * key, uint8_t key_len);

/** read_callback function is called from DB API to user module when read operation finishes.
 *  
 *  @param  err the return code for the read operation 
 *  @param  cb_arg the callback argument passed in when opening db_handle
 *  @param  key the key of a row. Its memory is freed by DB API
 *  @param  key_len the length of the key of a row
 *  @param  reply_data a list of columns for the row identified by the key
 *  @param  reply_data_num the number of columns returned */ 
typedef void (*db_read_callback_func_t) (db_error_t err,
                                         void *cb_arg, 
                                         uint8_t *key, uint8_t key_len, 
                                         db_column_t *reply_data, size_t reply_data_num);

/** Called from DB_API to user module when db_read_all()
 *  operation finishes. It is guaranteed to be called when db_read_all() is invoked()
 *
 *  @param  err the return code for the overall db_read_all operation
 *  @param  cb_arg the callback argument passed in when opening db_handle */
typedef void (*db_read_complete_callback_func_t) (db_error_t err,
                                                  void *cb_arg);

/** Called from DB_API to user module when there is error happening
 *  with the db connection. DB API will try to set up db connection when there is error with db
 *  connection. User module can choose to close the db handle upon receiving the notice
 *
 *  @param  cb_arg the callback argument passed in when opening db_handle
 *  @param  err the error code with the db connection */
typedef void (*db_ctrl_callback_func_t) (void *cb_arg, db_error_t err);

typedef enum {
    DB_CONNECT_LOCAL,
    DB_CONNECT_MASTER,
    DB_CONNECT_MAX
} db_connect_mode_t;

typedef enum {
  DB_NOTIFY_STANDBY_ONLY,
  DB_NOTIFY_ACTIVE_ONLY,
  DB_NOTIFY_ALWAYS,
  DB_NOTIFY_MAX
} db_notify_mode_t;

typedef void (*db_notify_callback_func_t) (
              db_error_t err, 
              db_id_t db_id,                
              void *cb_arg,                          
              uint8_t *key, 
              uint8_t key_len,
              db_column_t *columns, 
              size_t num_column);

#include "db_impl.h"

/** Get a db handle.
 *
 *  @param  db_id the DBTBLID enum. 
 *  @param  db_handle Holding the db_handle to return to user module.
 *  @param  cb_data the callback argument to be used later with callback functions.
 *  @param  ctrl_cb the function to handle ctrl event with db connection.
 *  @param  write_cb the function to handle write operation finish
 *  @param  read_cb the function to handle read operation finish
 *  @param  read_complete_cb the function to handle when db_read_all finishes */
db_error_t db_open(int32_t db_id, 
                   db_handle_t **db_handle,
                   void *cb_data,
                   db_ctrl_callback_func_t ctrl_cb,
                   db_write_callback_func_t write_cb,
                   db_read_callback_func_t read_cb,
                   db_read_complete_callback_func_t read_complete_cb);

/** Close a db handle.
 *
 *  @param  db_handle the db handle to close */
db_error_t db_close(db_handle_t *db_handle);


/** Write a single column into a row in DB
 * 
 *  @param  db_handle the db handle to use
 *  @param  key the key of the row
 *  @param  key_len the length of the key
 *  @param  column the pointer to the data of the column to be written into DB */
db_error_t db_write_single_column(db_handle_t *db_handle, 
                                  uint8_t* key, size_t key_len, 
                                  db_column_t* column);

/** Write a single column into a row in DB
 *
 *  @param update if set, key must already exist in DB. */
db_error_t db_write_single_column_ex(db_handle_t *db_handle, 
                                  uint8_t* key, size_t key_len, 
                                  db_column_t* column,
                                  int update);

/** Same as db_write_single_column except it cannot be used until after db_write_single_row.
 */
db_error_t db_write_single_column_fast(db_handle_t *db_handle, 
                                  uint8_t* key, size_t key_len, 
                                  db_column_t* column);

/** Write a single row into DB
 *
 *  @param db_handle the db handle to use
 *  @param key the key of the row
 *  @param key_len the length of the key
 *  @param columns the array list of columns to be written into DB
 *  @param column_num the number of columns in the column list */
db_error_t db_write_single_row(db_handle_t *db_handle, 
                               uint8_t* key, size_t key_len, 
                               db_column_t *columns, size_t column_num);

/** Write a single row into DB
 *
 *  @param update if set, key must already exist in DB. */
db_error_t db_write_single_row_ex(db_handle_t *db_handle, 
                               uint8_t* key, size_t key_len, 
                               db_column_t *columns, size_t column_num,
                               int update);

/** Delete a single row from DB
 * 
 *  @param db_handle the db handle to use
 *  @param key the key of the row
 *  @param key_len the length of the key */
db_error_t db_delete_single_row(db_handle_t *db_handle, 
                                uint8_t* key, size_t key_len);

/** Delete all rows from the current DB that db_handle points to.
 *
 *  @param db_handle the db handle to use */
db_error_t db_delete_all_row(db_handle_t *db_handle);

/** Read all the rows from the current DB
 *
 *  @param db_handle the db handle to use */
db_error_t db_read_all(db_handle_t *db_handle); 

/** Bind DB to a new IP address except 127.0.0.1
 *
 * @param ip address to bind to */
db_error_t db_bind_interface(char *addr);

/** Read a single row from DB
 * 
 *  @param db_handle the db handle to use
 *  @param key the key of the row
 *  @param key_len the length of the key */
db_error_t db_read_single_row(db_handle_t *db_handle, uint8_t* key, size_t key_len);

/** Read some columns of a single row from DB
 * 
 *  @param db_handle the db handle to use
 *  @param key the key of the row
 *  @param key_len the length of the key
 *  @param columns the array list of columns to be read from DB
 *         where each columns[i].column_id has to be specified.
 *  @param column_num the number of columns in the column list */
db_error_t db_read_single_row_columns(db_handle_t *db_handle,
                                      uint8_t* key, size_t key_len,
                                      db_column_t *columns, size_t column_num);

/** Called from DB API to user module when 
 *  the operation db_get_total_row_number() finishes.
 *
 *  @param err the return code for the query operation
 *  @param cb_arg the returned cb_arg 
 *  @param answer the integer result from query */
typedef void (*db_read_integer_callback_func_t) (db_error_t err, void *cb_arg, int answer);

/** Query the number of existing rows in the current DB
 *
 *  @param db_handle the db handle to use 
 *  @param cb the callback function to receive result
 *  @param cb_arg the cb_arg to be returned when cb is called */ 
db_error_t db_get_total_row_number(db_handle_t *db_handle, 
                                db_read_integer_callback_func_t cb,
                                void *cb_arg);

/** This function must be called before all other db api functions, it is called at vnfha_api_init*/
void db_api_init(void);

/** The function is called when remote Redis server address becomes available or changed.
    The function will detect if the info is redundant/repeated. */
void db_new_remote_server_ip(char *ip);

/** This function is called when the local node changes status from master to slave. 
    Note that by default, db_api assumes the local node is master. The function will detect 
    if the info is redundant/repeated. */
void db_is_slave(void);

/** This function is called when the local node changes status from slave to master. 
    The function will detect if the info is redundant/repeated. */
void db_is_master(void);




#endif
