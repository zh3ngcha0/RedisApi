#ifndef __VNF_LIMITS_H__
#define __VNF_LIMITS_H__

#define MSG_PER_GROUP   256
#define RECORD_PER_GROUP    65536
#define DBTBL_PER_GROUP    65536
#define DBALARMTBL_TASK_TYPE_PER_GROUP 32768 // > (TASK_NAME_MAX)
#define DBINST_PER_TBL   31
#define DBCOL_PER_TBL   256
#define VNF_SLICE_MAX   1024
#define VNF_VM_MAX      256
#define EXT_PER_GROUP   256

#if VNF_SLICE_MAX < VNF_VM_MAX
#error "number of slices should be greater than the number of VMs in the VNF instance"
#endif

#endif


