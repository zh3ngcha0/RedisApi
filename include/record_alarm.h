#ifndef __RECORD_GROUP_ALARM_H__
#define __RECORD_GROUP_ALARM_H__

/* This file defines all the record IDs within RECORD_GROUP_ALARM and db column IDs.
 * Define request and response message ID in a pair. i.e.  <n, n+1>
 * Naming convention: ALARM_WORD1_..._WORDn, multiple WORDs are alllowed
 * after the group name ALARM_
 */

#include "vnf_limits.h"
#include "record_group.h"

#undef DEFINE_ENUM
// adding prefix to avoid conflict
#define DEFINE_ENUM(name) ALARM_##name

typedef enum {
    /* enum for alarm data record type, start from 1 */
    DEFINE_ENUM(INDEX)   = 1,
    DEFINE_ENUM(LEVEL)      = 3,
    DEFINE_ENUM(TEXT)       = 4,
     
    DEFINE_ENUM(LAST_ONE)
    
}RECORDAlarm;
#ifndef TARGET_SMM
_Static_assert(DEFINE_ENUM(LAST_ONE) <= RECORD_PER_GROUP, L"Exceeding max number of messages per group, create a new group to continue");
#endif
#endif

