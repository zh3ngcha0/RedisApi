#ifndef __RECORD_CDB_ID_CM_H__
#define __RECORD_CDB_ID_CM_H__

/*-----------------------------------
 * CDB_ID_CM individual records
 *
 * cm_chan_assign
 * cm_chan_assign.us_chan_set
 *
 * us
 * ds
 * cm_attr_assign_suc
 * cm_ip_addr
 * cm_ipv6_addr
 * flags
 * ofdm_ds_profile_list
 * pload
 * vrf_id
 * aucDutCMIM
 * num_to_be_recovery_us
 *
 * 0..L_ddm_docsIfCmtsCmStatus_flags_len
 * 0..num_mcast_dsid offset
 * 0..mid offset
 *
 * loadbal3cmtscmparams
 * svfcnt - missing commented out
 * sub_mgt_filter_group
 * num_reseq_dsid
 * num_mcast_dsid
 *
 */

#define CDB_ID_CM_RECORDS \
    XX(CM_FLAGS) \
    XX(CM_STATUS_STATE) \
    XX(CM_NUM_MCAST_DSID) \
    XX(CM_NUM_RESEQ_DSID) \
    XX(CM_IS_DOING_DCC) \
    XX(CM_CHAN_ASSIGN) \
    XX(CM_CHAN_ASSIGN_US_CHAN_SET) \
    XX(CM_US) \
    XX(CM_DS) \
    XX(CM_ATTR_ASSIGN_SUC) \
    XX(CM_IP_ADDR) \
    XX(CM_IPV6_ADDR) \
    XX(CM_OFDM_DS_PROFILE_LIST) \
    XX(CM_PLOAD) \
    XX(CM_VRF_ID) \
    XX(CM_AUC_DUT_CMIM) \
    XX(CM_NUM_TO_BE_RECOVERY_US) \
    XX(CM_LOADBAL3_CMTS_CMP_PARAMS) \
    XX(CM_SUB_MGT_FILTER_GROUP) \
    XX(CM_KEY1_DOCSIF_CMTS_CM_STATUS_INDEX) \
    XX(CM_KEY2_DOCSIF_CMTS_CM_STATUS_MACADDR) \
    XX(CM_DDM_MAC_CM_BAK_DATA_REMAINING)  
    // always create primary and secondary key records 
    // as we will have corresponding column for key1 and key2 
    // This contains the remaining fields in the CDB_ID_CM data structure ddm_mac_cm_bak_data_t

#endif
