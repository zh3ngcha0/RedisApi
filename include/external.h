#ifndef EXTERNAL_H
#define EXTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <inttypes.h>
#include <protobuf-c/protobuf-c.h>
#include "db_api.h"
#include "db_group.h"
#include "db_common.h"

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

typedef int db_magic_t;
typedef int32_t db_id_t;



typedef int32_t DbTblID;
#define DBTBLID(group, tbl, inst) ((DbTblID)((DBTBLTYPEID(group, tbl) <<DBINST_BITS) | inst))
#define NULL_DBTBLID ((DbTblID)(-1))


#include <event2/event.h>


typedef void (*db_notify_callback_func_t) (
              db_error_t err,
              db_id_t db_id,
              void *cb_arg,
              uint8_t *key,
              uint8_t key_len,
              db_column_t *columns,
              size_t num_column);



/** Called from DB_API to user module when there is error happening
 *  with the db connection. DB API will try to set up db connection when there is error with db
 *  connection. User module can choose to close the db handle upon receiving the notice
 *
 *  @param  cb_arg the callback argument passed in when opening db_handle
 *  @param  err the error code with the db connection */
typedef void (*db_ctrl_callback_func_t) (void *cb_arg, db_error_t err);


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

#endif
