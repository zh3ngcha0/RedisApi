#ifndef __DB_COMMON_H__
#define __DB_COMMON_H__

#include <stdint.h>
#include "vnf_limits.h"

/* DbTblTypeID is not for external use. 
   DbTblID is used to build a DB handle, identifying a globally unique database.
   -  db table group (8 bits), db table type (16 bits), task instance (4 bits)
      4 bit task instance is temperary until we switch to name-space, see instances in vnf_init 
   DbTblColID is to identify 1) a column in table, and 2) user data type in protocol buffer 
   -  db table group(8 bits), db table type (16 bits); db table column (8 bits) 
*/

/* 256 = 2^8 */
#define DBTBL_TYPE_BITS 16
#if DBTBL_PER_GROUP != 65536
#error "Mismatch with vnf_limits.h"
#endif

#define DBALARMTBL_TASK_TYPE_BIT 15
#if DBALARMTBL_TASK_TYPE_PER_GROUP != 32768
#error "Mismatch with vnf_limits.h"
#endif

#define DBINST_BITS 5
#if DBINST_PER_TBL != 31  //Reserve the last instance number to indicate CDB 
#error "Mismatch with vnf_limits.h"
#endif

//DBTBL_GROUP_MAXNUM < (32 - DBTBL_TYPE_BITS - DBCOL_TYPE_BITS) = 8
#define DBCOL_TYPE_BITS 8
#if DBCOL_PER_TBL != 256
#error "Mismatch with vnf_limits.h"
#endif

typedef int32_t DbTblTypeID;

#define DBTBLTYPEID(group, tbl) ((DbTblTypeID)(((DBTBL_GROUP_##group)<<DBTBL_TYPE_BITS) | (DBTBL_##group##_##tbl)))

typedef int32_t DbTblID;
#define DBTBLID(group, tbl, inst) ((DbTblID)((DBTBLTYPEID(group, tbl) <<DBINST_BITS) | inst))
#define NULL_DBTBLID ((DbTblID)(-1))

typedef int32_t DbTblColID;
#define DBTBLCOLID(group, tbl, col) ((DbTblColID)((DBTBLTYPEID(group, tbl) <<DBCOL_TYPE_BITS) | (DBTBL_##group##_##tbl##_##col)))

#define NULL_DBTBLCOLID ((DbTblColID)(-1))

//Alarm table id and column id are constructed in the same scheme as above, only that it uses set/clear bit and task_type in table id, and task_inst in column id
#define DBALARMTBLTYPEID(tid, action) ((DbTblTypeID)(((DBTBL_GROUP_ALARM)<<DBTBL_TYPE_BITS) | (action<< DBALARMTBL_TASK_TYPE_BIT) | TASK_NAME(tid)))
#define DBALARMTBLID(tid, action)     ((DbTblID)(DBALARMTBLTYPEID(tid, action) <<DBINST_BITS | TASK_INSTANCE(tid)))
#define DBALARMTBLCOLID(tid, action, col) ((DbTblColID)((DBALARMTBLTYPEID(tid, action) <<DBCOL_TYPE_BITS) | (DBTBL_ALARM_COL_##col)))

//CDB DB does not need to differentiate instance, it uses a reserved instance number 
//CDB DB does not use group CDB (to compress the table id space) instead it uses the reserved instance number to indicate being CDB
#define DBTBL_RESERVED_CDB_INST DBINST_PER_TBL //Reserve the last instance number to indicate CDB 
//#define CDB_DBTBLTYPEID(group, tbl) (DBTBL_##group##_##tbl)
//#define CDB_DBTBLID(group, tbl) ((DbTblID)((CDB_DBTBLTYPEID(group, tbl) <<DBINST_BITS) | DBTBL_RESERVED_CDB_INST))

#define DBID_GET_GROUP(dbtbl_id) ((uint32_t)(dbtbl_id>>(DBTBL_TYPE_BITS+DBINST_BITS)))
#define DBID_GET_TYPE(dbtbl_id)  ((uint32_t)((dbtbl_id>>DBINST_BITS) & (DBTBL_PER_GROUP-1)))
#define COLID_GET_COL(col_id)    ((uint32_t)(col_id & (DBCOL_PER_TBL-1)))
#endif
