#ifndef __DB_API_COMMON_H__
#define __DB_API_COMMON_H__

uint32_t construct_hashkey_common(db_id_t dbtbl_id, uint8_t *key, uint32_t key_len, uint8_t *hashkey, uint32_t hashkey_len);
uint32_t construct_setkey_common(db_id_t dbtbl_id, uint8_t *setkey, uint32_t setkey_len);
#endif //__DB_API_COMMON_H__
