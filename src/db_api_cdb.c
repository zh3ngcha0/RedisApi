#include <hiredis.h>
#include <stdint.h>

#include "db_api_log.h"
#include "db_api_util.h"
#include "db_cdb.h"

#undef DBIDTONAME
#define DBIDTONAME(id) #id
#undef XX

const char *DBCdbTBL_name[] = {
#define XX(tag)  DBIDTONAME(DBTBL_CDB_##tag),
    DBCdbTBL_MAP(XX)
#undef XX
    "unknown name" //last line
};

const char *DBCdbIdCmColumn_name[] = {
#define XX(tag) DBIDTONAME(DBTBL_CDB_ID_CM_##tag),
    DBCdbIdCmColumn_MAP(XX)
#undef XX
    "unknown name" //last line
};

const char *DBCdbIdServiceGroupColumn_name[] = {
#define XX(tag) DBIDTONAME(DBTBL_CDB_ID_CM_##tag),
    DBCdbIdServiceGroupColumn_MAP(XX)
#undef XX
    "unknown name" //last line
};

