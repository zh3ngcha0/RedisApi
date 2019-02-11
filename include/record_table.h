#ifndef __RECORD_TABLE_H__
#define __RECORD_TABLE_H__

#include <protobuf-c/protobuf-c.h>
#include "record_common.h"
#include "record_group.h"

typedef size_t (*GET_PACKED_SIZE_FUNC)(void *message);
typedef size_t (*PACK_FUNC)(const void *message, uint8_t *out);
typedef void*  (*UNPACK_FUNC)(ProtobufCAllocator *alloc, size_t len, const uint8_t *data);
typedef void   (*FREE_UNPACKED_FUNC)(void *message, ProtobufCAllocator *alloc);

typedef struct _RecordMetaInfo {
    const char           *name;
    GET_PACKED_SIZE_FUNC  get_packed_size;
    PACK_FUNC             pack;
    UNPACK_FUNC           unpack;
    FREE_UNPACKED_FUNC    free_unpacked;
}RecordMetaInfo;


extern RecordMetaInfo record_meta_table[RECORD_GROUP_MAXNUM * RECORD_PER_GROUP];

RecordMetaInfo *lookup_record_meta_table(uint32_t record_id);
void record_meta_table_init(void);

#endif
