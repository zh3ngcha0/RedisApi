#ifndef __DB_API_SYNC_H__
#define __DB_API_SYNC_H__

#include "vnf_assert.h"
#include <inttypes.h>

#include <protobuf-c/protobuf-c.h>

#include "db_api.h"
#include "db_impl_sync.h"

db_error_t db_open_sync(db_id_t dbtble_id, db_handle_sync_param_t *params, db_handle_sync_t **db_handle_out);

db_error_t db_open_sync_ex(db_id_t dbtble_id, 
                            db_handle_sync_param_t *params, 
                            db_handle_sync_t **db_handle_out, 
                            db_connect_mode_t mode); 

db_error_t db_close_sync(db_handle_sync_t *db_handle);

db_error_t db_write_single_column_sync(db_handle_sync_t *db_handle, 
                                       uint8_t* key, uint32_t key_len, 
                                       db_column_t* column);

db_error_t db_write_single_row_sync(db_handle_sync_t *db_handle, 
                                    uint8_t* key, uint32_t key_len, 
                                    db_column_t *columns, size_t column_num);

db_error_t db_delete_single_row_sync(db_handle_sync_t *db_handle, 
                                     uint8_t* key, uint32_t key_len);

db_error_t db_delete_all_row_sync(db_handle_sync_t *db_handle);

db_error_t db_read_single_row_columns_sync(db_handle_sync_t *db_handle, 
                                           uint8_t* key, uint32_t key_len, 
                                           db_column_t *columns, size_t column_num);

db_error_t db_read_single_row_sync(db_handle_sync_t *db_handle, 
                                   uint8_t* key, uint32_t key_len, 
                                   db_column_t *columns, size_t column_num);

db_error_t db_free_read_sync(db_handle_sync_t *db_handle, 
                             uint8_t* key, uint32_t key_len, 
                             db_column_t *columns, size_t column_num);

db_error_t db_get_next_row_sync(db_handle_sync_t *db_handle, uint8_t* key, uint32_t key_len, uint32_t *out_key_len);

void db_abort_get_next_row_sync(db_handle_sync_t *db_handle);


/** Make DB slave to the master DB at addr
 *
 * @param master DB ip address */
db_error_t db_slave_of(char *addr, db_handle_sync_t *db_handle);

/** Make DB to become master DB
 *
 */
db_error_t db_become_master(db_handle_sync_t *db_handle);

/** On a standby, query if the local Redis DB has done initial sync from master 
 *
 *  @param *answer 0 if not done yet */ 
db_error_t db_check_slave_initial_replication_sync(db_handle_sync_t *db_handle, int *answer);

 
#endif
