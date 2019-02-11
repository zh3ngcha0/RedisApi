#ifndef __RECORD_GROUP_HA_H__
#define __RECORD_GROUP_HA_H__

/* This file defines all the record IDs within RECORD_GROUP_HA and db column IDs.
 * Define request and response message ID in a pair. i.e.  <n, n+1>
 * Naming convention: HA_WORD1_..._WORDn, multiple WORDs are alllowed
 * after the group name HA_
 */

#include "vnf_limits.h"
#include "record_group.h"

#undef DEFINE_ENUM
// adding prefix to avoid conflict
#define DEFINE_ENUM(name) HA_##name

typedef enum {
    /* enum for ipsec ha data, start from 1 */
    HA_IPSEC_START_TIME  = 1,
    HA_IKE_PEER  = 2,
    HA_IKE_INFO = 3,
    HA_IKE_EPDG_CTX = 4,
    HA_IKE_STAT = 5,
    HA_CHILD_INFO = 6,
    HA_CHILD_KEY_EX  = 7,
    HA_CHILD_STAT = 8,
 
    /* enum for rsrcMgr ippool data: */
    HA_IPPOOL = 9,

    /* enum for epdg's swm: */
    HA_SWM_DIAM = 10,

    /*enum for HenbgwS1mmemgrInstanceData*/
    HA_S1MME_INST_CFG = 11,
    
    /* enum for epdg's session: */
    HA_EPDG_SESSION_COMMON = 12,
    HA_EPDG_SESSION_UE = 13,
    HA_EPDG_SESSION_APN_AMBR = 14,

    /* enum for epdg's bearer: */
    HA_EPDG_BEARER_COMMON = 15,
    HA_EPDG_BEARER_QOS = 16,

    /*enum for HenbgwS1mmemgrLenbData*/
    HA_S1MME_LENB = 17,
    
    /*enum for HenbgwS1mmemgrMmeCbData*/
    HA_S1MME_MME_CB = 18,
    /*enum for HenbgwS1mmemgrMmePlmnData*/
    HA_S1MME_MME_PLMN = 19,
    /*enum for HenbgwS1mmemgrMmeMmecsData*/
    HA_S1MME_MME_MMECS = 20,
    /*enum for HenbgwS1mmemgrMmeGroupidData*/
    HA_S1MME_MME_GROUPID = 21,
    
    /*enum for HenbgwS1mmemgrSuptdTaiData*/
    HA_S1MME_SUPTD_TAI = 22,
    
    /*enum for HenbgwS1mmemgrHenbCbData*/
    HA_S1MME_HENB_CB = 23,
    
    /*enum for HenbgwS1mmemgrX2HenbCbData*/
    HA_S1MME_X2_HENB = 24,
    
    /*enum for HenbgwS1mmemgrX2EnbCbData*/
    HA_S1MME_X2_ENB = 25,
    
    /*enum for HenbgwS1mmemgrX2ServedCellData*/
    HA_S1MME_X2_SERVED_CELL = 26, 

    /*enum for henbgw henbmgr*/
    HA_HENBM_INST_CFG = 27,    
    HA_HENB_DATA_CB = 28,
    HA_HENB_DATA_CSG = 29,
    HA_HENB_DATA_TAI = 30,
    HA_UE_DATA_CB = 31,
    HA_TE_DATA_CB = 32,    
    HA_TE_GTPU_STAT = 33,    
    HA_ERAB_QOS = 34,    
    HA_GTPU_STAT = 35,

    /*enum for HenbgwS1mmemgrSCTP*/
    HA_S1MME_SCTP_RED_GLB_DATA = 36,
    HA_S1MME_SCTP_RED_GLB_EP   = 37, 
    HA_S1MME_SCTP_RED_GLB_PMTU = 38,     
    HA_S1MME_SCTP_RED_EP_DATA       = 39,
    HA_S1MME_SCTP_RED_EP_ASSOCOBJ   = 40,
    
    /*enum for x2henbdata*/
    HA_X2_HENB_DATA_CB = 41,
    
    HA_HENB_SCTP_RED_GLB_DATA = 42,
    HA_HENB_SCTP_RED_GLB_EP   = 43, 
    HA_HENB_SCTP_RED_GLB_PMTU = 44,     
    HA_HENB_SCTP_RED_EP_DATA       = 45,
    HA_HENB_SCTP_RED_EP_ASSOCOBJ   = 46,

    // HnbGW Database
    HA_HGW_EXAMPLE = 47,
    HA_HGW_MULCOL_EXAMPLE_ID = 48,
    HA_HGW_MULCOL_EXAMPLE_NAME = 49,
    HA_HGW_MULCOL_EXAMPLE_NODE = 50,
    HA_HGW_SB_GLOBAL_CB = 51,
    HA_HGW_SB_SCT_SAP_CB = 52,
    HA_HGW_SB_TRS_SAP_CB = 53,
    HA_HGW_SB_ENDP_CB = 54,
    HA_HGW_SB_ASSO_CB = 55,

    HA_HGW_IT_GLB_CB = 56,
    HA_HGW_IT_GLB_STS= 57,
    HA_HGW_IT_ASP_CB = 58,
    HA_HGW_IT_NWK_CB = 59,
    HA_HGW_IT_NSAP_CB = 60,    
    HA_HGW_IT_SCTSAP_CB = 61,
    HA_HGW_IT_PSP_CB = 62,
    HA_HGW_IT_PSP_UPD_STA = 63,
    HA_HGW_IT_ASSOC_CB = 64,
    HA_HGW_IT_PS_CB = 65,
    HA_HGW_IT_PS_UPD_STA = 66,
    HA_HGW_IT_PS_CFG_CB = 67,
    HA_HGW_IT_PS_ENDP_PSPSTACB = 68,
    HA_HGW_IT_PS_ENDP_ACTPSPCB = 69,
    HA_HGW_IT_PS_ENDP_RCTXCB = 70,
    HA_HGW_IT_RTE_CB = 71,
    HA_HGW_IT_DPC_STA_CB = 72,
    HA_HGW_IT_DPC_CB = 73,
    HA_HGW_IT_SLS_CB = 74,
    HA_HGW_IT_REV_UPD = 75,
    HA_HGW_HM_PEER_CB = 76,
    HA_HGW_HM_UE_CB = 77,
    HA_HGW_SP_SAP_CB = 78,

    HA_HGW_HI_GLOBAL_CB = 79,
    HA_HGW_HI_SAP_CB = 80,
    HA_HGW_SP_NSAP_CB = 81,
    HA_HGW_SP_RTE_CB = 82,
    HA_HGW_SP_PRIM_CB = 83,
    HA_HGW_SP_SCLI_CB = 84,
    HA_HGW_SP_CON_CB = 85,
    HA_HGW_SP_QOS = 86,
    HA_HGW_SP_CON_SIDE = 87,
    HA_HGW_SP_CON_ID = 88,
    HA_HGW_SP_ADDR = 89,
    HA_HGW_SP_SUA_ADDR = 90,
    HA_HGW_SP_GLB_TI = 91,
    HA_HGW_HI_CONN_CB = 92,

    HA_HGW_SB_ADDR_PORT_CB = 93,
    HA_HGW_SB_LOCAL_ADDR_CB = 94,
    HA_HGW_SB_ASSO_SEQ_CB = 95,
    HA_HGW_SB_ASSO_ACK_CB = 96,
    HA_HGW_SP_XUD_CB = 97,
    HA_HGW_HM_LOWER_SAP_CB = 98,
    HA_HGW_HM_IP_ADDR = 99,
    HA_HGW_HM_RUT_UPPER_SAP_CB = 100,
    HA_HGW_HM_HNT_UPPER_SAP_CB = 101,

    HA_HGW_RN_PCB_CB      = 102,
    HA_HGW_RN_USAP_CB     = 103,
    HA_HGW_RN_LSAP_CB     = 104,
    HA_HGW_RN_RNC_CB      = 105,
    HA_HGW_RN_CN_CB       = 106,
    HA_HGW_RN_CON_CB      = 107,
    
    HA_HGW_SB_ASSO_TSN    = 110,
    HA_HGW_RU_GLOBAL_CB   = 111,
    HA_HGW_RU_LOW_SAP_CB  = 112,
    HA_HGW_RU_HNB_CB      = 113,
    HA_HGW_RU_UE_CB       = 114,
    
    HA_HGW_GT_TPT_SAP_CB      = 115,
    HA_HGW_GT_GGU_SAP_CB      = 116,
    HA_HGW_GT_TPT_SRV_CB      = 117,
    HA_HGW_GT_CON_CB          = 118,
    HA_HGW_GT_MS_CB           = 119,
    HA_HGW_GT_GTPU_TUN_CB     = 120,
    HA_HGW_GT_CM_TPT_ADDR     = 121,
    HA_HGW_GT_CON_KEY         = 122,

    HA_HGW_HR_TSAP_CB         = 123,
    HA_HGW_HR_SSAP_CB         = 124,
    HA_HGW_HR_SESSION_CB      = 125,
    HA_HGW_HR_TPT_CB          = 126,
    HA_HGW_HR_MEMBER_CB       = 127,

    
    HA_HGW_UP_RTP_INFO_CB     = 128,
    HA_HGW_UP_RTP_TSAP_CB     = 129,
    HA_HGW_UP_GTP_INFO_CB     = 130,
    HA_HGW_UP_GTP_TSAP_CB     = 131,

    HA_HGW_SM_GLOBAL_CB       = 132,
    HA_HGW_SM_ASSOC_INFO      = 133,

    HA_HGW_RU_UE_RAB_CB      = 134,
    HA_HGW_RU_UE_RABS        = 135,
    HA_HGW_RU_GLOBAL_CN_CB   = 136,
    HA_HGW_RN_PEER_CB        = 137,
    HA_HGW_RN_CTX_CB         = 138,
    /*enum for gtpctrl*/
    HA_GTPCTRL_NODE_PEER_IP  = 139,
    HA_GTPCTRL_NODE_TYPE     = 140,
    HA_RESTART_COUNT_NUMBER  = 141,

    //add by djun for HR connCb
    HA_HGW_HR_CONN_NODE       = 142,

    /*enum for wifi data*/
    HA_WAG_UE_SESSION_OBJ     = 143,
    HA_RADIUS_SESSIONOBJ      = 144,
    HA_DHCP_LEASE_RECORD      = 145,
    HA_CGNAT_IP_POOL_RUNNING_STATUS = 146,
    HA_CGNAT_IP_POOL_RUNNING_SLOT_DATA = 147,
    HA_CGNAT_HA_UE_DATA = 148,
    HA_CGNAT_HA_UE_PORT_RANGE_ARRAY = 149,
    HA_CGNAT_HA_UE_EXT_PORT_RANGE_CB = 150,
    HA_CGNAT_HA_UE_ALG_PORT_RANGE_ARRAY = 151,
    HA_ACCOUNTING_IKE   = 152,
    HA_AAA_MGR_DIAM   = 153,

    HA_CH_ROW_MSG = 154,

    HA_LAST_ONE
}RECORDHa;

#if DEFINE_ENUM(LAST_ONE) > RECORD_PER_GROUP
#error "Exceeding max number of messages per group, create a new group to continue"
#endif

#endif
