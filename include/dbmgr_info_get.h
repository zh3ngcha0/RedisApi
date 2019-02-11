#include <dbmgr.h>
#include "dbmgr_impl.h"
#include <scheduler.h>

#include <msg_dbmgr.h>
#include <msg_dbmgr.pb-c.h>
#include "db_api.h"

#define LTEGW_DBMGR_MSG_GET_HA_IPSEC_TOTAL_NUM     0
#define LTEGW_DBMGR_MSG_GET_HA_IPSEC_BY_KEY            1

typedef struct _ltegw_dbmgr_msg_handle_s{
    MSGHandle mh;
    db_handle_t  *db_handle;
    uint8_t msg_type;
#define LTEGW_DBMGR_FREE     0
#define LTEGW_DBMGR_BUZY     1
    uint8_t         dbmgr_show_state;//dbmgr state :1 for buzy;0 for common
    union{
        DbmgrTotalCountResponse            dbmgr_ha_total_count_rsp; 
    }msg;
} ltegw_dbmgr_msg_handle_t;

void dbmgr_ha_sa_ctrl_callback(void *cb_arg, db_error_t err);

void dbmgr_ha_sa_write_callback(db_error_t err,  void *cb_arg,  uint8_t * key, uint8_t key_len);

void dbmgr_ha_sa_read_callback(db_error_t err,  void *cb_arg,
                                uint8_t * key, uint8_t key_len,  db_column_t *reply_data, size_t reply_data_num);

void dbmgr_ha_sa_read_complete_callback(db_error_t err, void *cb_arg) ;

void dbmgr_get_ha_total_count_callback (db_error_t err, void *cb_arg, int answer);

