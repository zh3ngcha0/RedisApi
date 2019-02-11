#include <string.h>
#include "db_api.h"
#include "db_api_sync.h"
#include "db_api_common.h"

// Defined as Upper 4 bits reserved, lower 4 bits redis DB API version 
// 
// bit7..bit4 bit3..0
// reserved   redis DB API version
//
// Increment this version number if REDIS HASHKey scheme is altered
// or any other additional REDIS features added that would require
// a separate DB instance or keyspace and might cause conflict with
// previous HASHKey schemes
uint8_t db_api_version = 1;

uint32_t construct_hashkey_common(db_id_t dbtbl_id, uint8_t *key, uint32_t key_len, uint8_t *hashkey, uint32_t hashkey_len) 
{
  // construct the hash key 
  // hash key has the form  
  //  
  // <db_api_version><dbtbl_id><key>
  //  2 chars         8 chars 
  //  
  //  2 chars db_api_version contain Hex chars of 1 byte db_api_version
  //  8 chars dbtbl_id contain Hex chars of 4 byte dbtbl_id 
  //  
  //
  char hashkeytag[128];
  uint32_t taglen;

  sprintf(hashkeytag, "%02x%08x", db_api_version, dbtbl_id);
  taglen = strlen(hashkeytag);
  VNF_ASSERT(taglen == DB_CMD_HASHKEY_TAG_LEN);

  VNF_ASSERT(hashkey_len >= (taglen+key_len));

  memcpy(hashkey, hashkeytag, taglen);
  memcpy(hashkey+taglen, key, key_len);

  return (taglen+key_len);
}

uint32_t construct_setkey_common(db_id_t dbtbl_id, uint8_t *setkey, uint32_t setkey_len) 
{
  char *tag = "DBTBLSET_";
  char keystr[128];
  int len1, len2;

  sprintf(keystr, "%02x%08x", db_api_version, dbtbl_id);

  len1 = strlen(keystr);
  len2 = strlen(tag);
  //printf("keystr %s len %d , tag %s len %d\n", keystr, len1, tag, len2);

  assert(setkey_len > (len1+len2));

  sprintf(setkey, "%s%s", tag, keystr);

  return (strlen(setkey));
}
