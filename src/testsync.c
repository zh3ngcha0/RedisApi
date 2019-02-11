#include "db_api.h"
#include "db_api_sync.h"
#include <glib.h>
#include <string.h>
#include <unistd.h>

#include "db_common.h"
#include "db_ha.h"

#include "record_common.h"
#include "record_ha.h"
#include "record_table.h"
#include "scheduler.h"

#include "record_ha.pb-c.h"

#include <stdlib.h>
#include <hiredis/adapters/libevent.h>

void testsync_write_cb (db_error_t err,
                    void *cb_arg, 
                    uint8_t * key, uint8_t key_len)
{
    printf("in %s Receive\n", __FUNCTION__);
}

void testsync_read_cb (db_error_t err,
                   void *cb_arg, 
                   uint8_t *key, uint8_t key_len, 
                   db_column_t *reply_data, size_t reply_data_num)
{
    printf("in %s Receive\n", __FUNCTION__);
}

void testsync_read_complete_cb (db_error_t err,
                            void *cb_arg)
{
    printf("in %s Receive\n", __FUNCTION__);
}

void testsync_ctrl_cb (void *cb_arg, db_error_t err)
{
    printf("in %s Receive %s\n", __FUNCTION__, db_error_str(err));
}



int testsyncmain(char *server_ip, int port, bool interact)
{
    db_handle_sync_t * db_handle;

    db_handle_sync_param_t param;
    int key1, i;
    db_column_t db_child_columns[2];
    HaChildInfo* bkChildInfo; 

    record_meta_table_init();

    memset(&param, sizeof(param), 0);

    strcpy((char *)&param.server_ip, server_ip);
    param.server_port = port;

    VNF_ASSERT((db_open_sync(1, &param, &db_handle) == DB_SUCCESS)); 
    printf("db_open_sync() complete\n");

    if (interact) {
      printf("press any key to continue\n");
      getchar();
    }
    

    bkChildInfo = (HaChildInfo*) g_malloc0(sizeof(HaChildInfo));
    if(!bkChildInfo) { 
      printf("Fail to allocate HaChildInfo");
      return 0;
    } 
    ha_child_info__init(bkChildInfo);

    bkChildInfo->has_authkey = 1;
    bkChildInfo->authkey.data = g_malloc0(5);
    memset(bkChildInfo->authkey.data, 0x33, 5);
    bkChildInfo->authkey.len = 5;

    bkChildInfo->key = g_malloc0(sizeof(HaChildKeyEx));
    ha_child_key_ex__init(bkChildInfo->key);
    bkChildInfo->key->dwid = 0x789a;

    db_child_columns[0].column_id    = DBTBLCOLID(HA, SA, CHILD_INFO_IB);
    db_child_columns[0].record_type  = RECORDID(HA, CHILD_INFO);
    db_child_columns[0].record       = (ProtobufCMessage*) bkChildInfo;

    db_child_columns[1].column_id    = DBTBLCOLID(HA, SA, CHILD_INFO_OB);
    db_child_columns[1].record_type  = RECORDID(HA, CHILD_INFO);
    db_child_columns[1].record       = (ProtobufCMessage*) bkChildInfo;

    for (i =0; i < 64; i++) {
      /* test write_single_row */
      key1 = 1236+i;
    
      VNF_ASSERT((db_write_single_row_sync(db_handle, (void*)&key1, sizeof(int), db_child_columns, 1) == DB_SUCCESS));

      printf("db_write_single_row_sync() complete row# %d\n", i);
    }

    if (interact) {
      printf("press any key to continue\n");
      getchar();
    }
    i = 0;
    while (i++ < 1000) {
      int nextkey, keylen, outkeylen;
      keylen=sizeof(nextkey);
      nextkey=0;
      VNF_ASSERT((db_get_next_row_sync(db_handle, (void *)&nextkey, keylen, &outkeylen) == DB_SUCCESS));

      if (!outkeylen) {
        printf("db_get_next_row_sync() end of table\n");
        break;
      }

      printf("db_get_next_row_sync() complete row# %d key %d keylen %d\n", i, nextkey, outkeylen);
    } 

    db_column_t one_column;
 
    bkChildInfo->authkey.data = g_malloc0(5);
    memset(bkChildInfo->authkey.data, 0x66, 5);

    one_column.column_id    = DBTBLCOLID(HA, SA, CHILD_INFO_IB);
    one_column.record_type  = RECORDID(HA, CHILD_INFO);
    one_column.record       = (ProtobufCMessage*) bkChildInfo;
 
    VNF_ASSERT((db_write_single_column_sync(db_handle, (void*)&key1, sizeof(int), &one_column) == DB_SUCCESS));
    printf("db_write_single_column_sync() complete\n");

    if (interact) {
      printf("press any key to continue\n");
      getchar();
    }

    // we are reading back the record - it must be set to NULL
    db_child_columns[0].record = NULL;
    VNF_ASSERT((db_read_single_row_columns_sync(db_handle,(void*)&key1, sizeof(int), db_child_columns, 1) == DB_SUCCESS));
    VNF_ASSERT((db_free_read_sync(db_handle,(void*)&key1, sizeof(int), db_child_columns, 1) == DB_SUCCESS));

    printf("db_read_single_row_columns_sync() complete\n");
    if (interact) {
      printf("press any key to continue\n");
      getchar();
    }

    db_child_columns[0].record = NULL;
    VNF_ASSERT((db_read_single_row_sync(db_handle,(void*)&key1, sizeof(int), db_child_columns, 1) == DB_SUCCESS));
    VNF_ASSERT((db_free_read_sync(db_handle,(void*)&key1, sizeof(int), db_child_columns, 1) == DB_SUCCESS));
    printf("db_read_single_row_sync() complete\n");
    if (interact) {
      printf("press any key to continue\n");
      getchar();
    }

    VNF_ASSERT((db_delete_single_row_sync(db_handle, (void*)&key1, sizeof(int)) == DB_SUCCESS));
    printf("db_delete_single_row_sync() complete\n");
    if (interact) {
      printf("press any key to continue\n");
      getchar();
    }

    VNF_ASSERT((db_close_sync(db_handle) == DB_SUCCESS));
    printf("db_close_sync() complete\n");

    if (interact) {
      printf("press any key to continue\n");
      getchar();
    }

    printf("testsyncmain complete, Goodbye!\n");
    return 0;
}
