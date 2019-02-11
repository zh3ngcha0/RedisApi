#ifndef __DB_HA_H__
#define __DB_HA_H__

/* This file defines all the database table IDs and column IDs within DBTBL_GROUP_HA
 * Naming convention: HA_WORD1_..._WORDn, multiple WORDs are alllowed
 * after the group name HA_
 */

#include "vnf_limits.h"
#include "db_group.h"

/************************* database tables *****************************************/

typedef enum {
    DBTBL_HA_IPSEC_START_TIME, 
    DBTBL_HA_SA,               
    DBTBL_HA_IPPOOL,           
    DBTBL_HA_SWM_DIAM,         
    DBTBL_HA_EPDG_SESSION,     
    DBTBL_HA_EPDG_BEARER,      
    DBTBL_HA_S1MME_INST_CFG,   
    DBTBL_HA_S1MME_LENB,       
    DBTBL_HA_S1MME_MME,        
    DBTBL_HA_S1MME_SUPTD_TAI,  
    DBTBL_HA_S1MME_HENB_CB,    
    DBTBL_HA_S1MME_X2_HENB,             
    DBTBL_HA_S1MME_X2_ENB,              
    DBTBL_HA_S1MME_X2_SERVED_CELL,      
    DBTBL_HA_HENBM_INST_CFG,    
    DBTBL_HA_HENB_DATA,         
    DBTBL_HA_UE_DATA,           
    DBTBL_HA_TE_DATA,           
    DBTBL_HA_S1MME_SCTP_RED_GLB,        
    DBTBL_HA_S1MME_SCTP_RED_EP,         
    DBTBL_HA_X2_HENB_DATA,      
    DBTBL_HA_TE_GTPU,                  
    DBTBL_HA_HENB_SCTP_RED_GLB,        
    DBTBL_HA_HENB_SCTP_RED_EP,         
    DBTBL_HA_LAST_ONE,         
}DBHaTBL;

#if DEFINE_ENUM(LAST_ONE) > DBTBL_PER_GROUP
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of IPSEC_START_TIME table *********************/
//Key is SIT of the task instance
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_IPSEC_START_TIME_##name

#define DBHaIpsecStartTimeColumn_MAP(XX)     \
 /* enum or name */                \
   XX(VALUE)                     \
   XX(LAST_ONE)                  \
   //end of map

typedef enum {
    /* enum for columns in IPsec Start Time table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaIpsecStartTimeColumn_MAP(XX)
  #undef XX
}DBHaIpsecStartTimeColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of SA table ***********************************/
//Key is sa_id
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_SA_##name

typedef enum {
    DBTBL_HA_SA_IKE_PEER,               
    DBTBL_HA_SA_IKE_INFO,                
    DBTBL_HA_SA_IKE_STAT,                
    DBTBL_HA_SA_CHILD_INFO_IB,           
    DBTBL_HA_SA_CHILD_INFO_OB,           
    DBTBL_HA_SA_CHILD_STAT_IB,           
    DBTBL_HA_SA_CHILD_STAT_OB,           
    DBTBL_HA_SA_LAST_ONE,                
}DBHaSaColumn;


#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif
/********************* database columns of IPPOOL table ***********************************/
/* The Key is DBHaIppoolKey: pool_name+pool_index+allocated_ip_addr */

#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_IPPOOL_##name

#define DBHaIppoolColumn_MAP(XX)        \
  /* enum or name */                    \
   XX(ADDR)                            \
   XX(LAST_ONE)                         \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaIppoolColumn_MAP(XX)
  #undef XX

}DBHaIppoolColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif
/********************* database columns of SWM_DIAM table ***********************************/
//Key is SIT of the task instance
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_SWM_DIAM_##name

#define DBHaSwmDiamColumn_MAP(XX)  \
  /* enum or name */               \
   XX(ID)                          \
   XX(LAST_ONE)                    \
   //end of map

typedef enum {
    /* enum for columns in HA_SWM_DIAM table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaSwmDiamColumn_MAP(XX)
  #undef XX

}DBHaSwmDiamColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif
/********************* database columns of S1mmeInst table ***********************************/
/**/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_S1MME_INST_CFG_##name

#define DBHaS1mmeInstCfgColumn_MAP(XX)        \
  /* enum or name */                    \
   XX(ID)                               \
   XX(LAST_ONE)                         \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaS1mmeInstCfgColumn_MAP(XX)
  #undef XX

}DBHaS1mmeInstCfgColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of S1mmeLenb table ***********************************/
/**/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_S1MME_LENB_##name

#define DBHaS1mmeLenbColumn_MAP(XX)    \
  /* enum or name */                   \
   XX(LENB_INDEX)                      \
   XX(LAST_ONE)                        \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaS1mmeLenbColumn_MAP(XX)
  #undef XX

}DBHaS1mmeLenbColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of S1mmeMme table ***********************************/
/**/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_S1MME_MME_##name

#define DBHaS1mmeMmeColumn_MAP(XX)   \
  /* enum or name */                  \
   XX(CB)                            \
   XX(PLMN)                          \
   XX(MMECS)                         \
   XX(GROUPID)                       \
   XX(LAST_ONE)                      \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaS1mmeMmeColumn_MAP(XX)
  #undef XX

}DBHaS1mmeMmeColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of S1MME_SUPTD_TAI table ***********************************/
/**/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_S1MME_SUPTD_TAI_##name

#define DBHaS1mmeSuptdTaiColumn_MAP(XX)   \
  /* enum or name */                  \
   XX(VALUE)                         \
   XX(LAST_ONE)                      \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaS1mmeSuptdTaiColumn_MAP(XX)
  #undef XX

}DBHaS1mmeSuptdTaiColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of S1MME_Henb_Cb table ***********************************/
/**/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_S1MME_HENB_CB_##name

#define DBHaS1mmeHenbCbColumn_MAP(XX)   \
  /* enum or name */                  \
   XX(VALUE)                         \
   XX(LAST_ONE)                      \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaS1mmeHenbCbColumn_MAP(XX)
  #undef XX

}DBHaS1mmeHenbCbColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of S1MME_X2_HENB table ***********************************/
/**/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_S1MME_X2_HENB_##name

#define DBHaS1mmeX2HenbColumn_MAP(XX)   \
  /* enum or name */                  \
   XX(VALUE)                         \
   XX(LAST_ONE)                      \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaS1mmeX2HenbColumn_MAP(XX)
  #undef XX

}DBHaS1mmeX2HenbColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of S1MME_X2_ENB table ***********************************/
/**/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_S1MME_X2_ENB_##name

#define DBHaS1mmeX2EnbColumn_MAP(XX)   \
  /* enum or name */                  \
   XX(VALUE)                         \
   XX(LAST_ONE)                      \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaS1mmeX2EnbColumn_MAP(XX)
  #undef XX

}DBHaS1mmeX2EnbColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif


/********************* database columns of S1MME_X2_SERVED_CELL table ***********************************/
/**/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_S1MME_X2_SERVED_CELL_##name

#define DBHaS1mmeX2ServedCellColumn_MAP(XX)   \
  /* enum or name */                  \
   XX(VALUE)                         \
   XX(LAST_ONE)                      \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaS1mmeX2ServedCellColumn_MAP(XX)
  #undef XX

}DBHaS1mmeX2ServedCellColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of S1MME_RED_SCTP_GLB table ***********************************/
/**/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_S1MME_SCTP_RED_GLB_##name 

#define DBHaS1mmeSctpRedGlbColumn_MAP(XX)   \
  /* enum or name */                  \
   XX(DATA)                          \
   XX(EP)                            \
   XX(PMTU)                          \
   XX(LAST_ONE)                      \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaS1mmeSctpRedGlbColumn_MAP(XX)
  #undef XX

}DBHaS1mmeSctpRedGlbColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of S1MME_RED_SCTP_EP table ***********************************/
/**/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_S1MME_SCTP_RED_EP_##name 

#define DBHaS1mmeSctpRedEpColumn_MAP(XX)   \
  /* enum or name */                  \
   XX(DATA)                          \
   XX(ASSOCOBJ)                      \
   XX(LAST_ONE)                      \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaS1mmeSctpRedEpColumn_MAP(XX)
  #undef XX

}DBHaS1mmeSctpRedEpColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif


/********************* database columns of EPDG_SESSION table ***********************************/
//Key is gtpc_teid of the epdg session
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_EPDG_SESSION_##name

#define DBHaEpdgSessionColumn_MAP(XX)  \
  /* enum or name */               \
   XX(COMMON)                      \
   XX(UE)                          \
   XX(APN_AMBR)                    \
   XX(LAST_ONE)                    \
   //end of map

typedef enum {
    /* enum for columns in HA_SWM_DIAM table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaEpdgSessionColumn_MAP(XX)
  #undef XX

}DBHaEpdgSessionColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of EPDG_BEARER table ***********************************/
//Key is gtpu_teid of the epdg bearer
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_EPDG_BEARER_##name

#define DBHaEpdgBearerColumn_MAP(XX)  \
  /* enum or name */               \
   XX(COMMON)                      \
   XX(QOS)                         \
   XX(LAST_ONE)                    \
   //end of map

typedef enum {
    /* enum for columns in HA_SWM_DIAM table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaEpdgBearerColumn_MAP(XX)
  #undef XX

}DBHaEpdgBearerColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/*********************database columns of HENBM_INST table *************************/
// key is inst_id
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_HENBM_INST_CFG_##name

#define DBHaHenbmInstCfgColumn_MAP(XX)  \
  /* enum or name */              \
   XX(ID)                         \
   XX(LAST_ONE)                    \
   //end of map

typedef enum {
    /* enum for columns in HA_HENBM_INST_CFG table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaHenbmInstCfgColumn_MAP(XX)
  #undef XX

}DBHaHenbmInstCfgColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/*******************database columns of HENB_DATA table**************/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_HENB_DATA_##name

#define DBHaHenbDataColumn_MAP(XX)  \
  /* enum or name */              \
   XX(CB)                         \
   XX(CSG)                         \
   XX(TAI)                         \
   XX(LAST_ONE)                    \
   //end of map

typedef enum {
    /* enum for columns in HA_HENBM_INST_CFG table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaHenbDataColumn_MAP(XX)
  #undef XX

}DBHaHenbDataColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/************************* database columns of UE_DATA table *********************/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_UE_DATA_##name

#define DBHaUeDataColumn_MAP(XX)  \
  /* enum or name */              \
   XX(CB)                         \
   XX(LAST_ONE)                    \
   //end of map

typedef enum {
    /* enum for columns in HA_HENBM_INST_CFG table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaUeDataColumn_MAP(XX)
  #undef XX

}DBHaUeDataColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/************************* database columns of TE_DATA table *********************/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_TE_DATA_##name

#define DBHaTeDataColumn_MAP(XX)  \
  /* enum or name */              \
   XX(CB)                         \
   XX(LAST_ONE)                    \
   //end of map

typedef enum {
    /* enum for columns in HA_HENBM_INST_CFG table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaTeDataColumn_MAP(XX)
  #undef XX

}DBHaTeDataColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/*******************database columns of X2_HENB_DATA table**************/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_X2_HENB_DATA_##name

#define DBHaX2HenbDataColumn_MAP(XX)  \
  /* enum or name */              \
   XX(CB)                         \
   XX(LAST_ONE)                    \
   //end of map

typedef enum {
    /* enum for columns in X2_HENB_DATA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaX2HenbDataColumn_MAP(XX)
  #undef XX

}DBHaX2HenbDataColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/*******************database columns of TE_GTPU_STAT table**************/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_TE_GTPU_##name

#define DBHaTeGtpuColumn_MAP(XX)  \
  /* enum or name */              \
   XX(STAT)                         \
   XX(LAST_ONE)                    \
   //end of map

typedef enum {
    /* enum for columns in X2_HENB_DATA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaTeGtpuColumn_MAP(XX)
  #undef XX

}DBHaTeGtpuColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of HENB_RED_SCTP_GLB table ***********************************/
/**/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_HENB_SCTP_RED_GLB_##name 

#define DBHaHenbSctpRedGlbColumn_MAP(XX)   \
  /* enum or name */                  \
   XX(DATA)                          \
   XX(EP)                            \
   XX(PMTU)                          \
   XX(LAST_ONE)                      \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaHenbSctpRedGlbColumn_MAP(XX)
  #undef XX

}DBHaHenbSctpRedGlbColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

/********************* database columns of HENB_RED_SCTP_EP table ***********************************/
/**/
#undef DEFINE_ENUM // adding prefix to avoid conflict
#define DEFINE_ENUM(name) DBTBL_HA_HENB_SCTP_RED_EP_##name 

#define DBHaHenbSctpRedEpColumn_MAP(XX)   \
  /* enum or name */                  \
   XX(DATA)                          \
   XX(ASSOCOBJ)                      \
   XX(LAST_ONE)                      \
   //end of map

typedef enum {
    /* enum for columns in HA_SA table*/
  #define XX(tag) DEFINE_ENUM(tag),
    DBHaHenbSctpRedEpColumn_MAP(XX)
  #undef XX

}DBHaHenbSctpRedEpColumn;

#if DEFINE_ENUM(LAST_ONE) > DBCOL_PER_TBL
#error "Exceeding max number of table per group, create a new group to continue"
#endif

#endif
