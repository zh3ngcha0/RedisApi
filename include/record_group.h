#ifndef __RECORD_GROUP_H__
#define __RECORD_GROUP_H__

/* This is the master header file that defines the database record_type groups.
 * Each individual group will have its own header file that defines all the record IDs
 * within that group.
 * Start from 0, increment by one, don't jump.
 * Naming convention: RECORD_GROUP_WORD, only ONE WORD after the underscore.
 */

#undef DEFINE_RECORD_ENUM
// adding prefix to avoid conflict
#define DEFINE_RECORD_ENUM(name) RECORD_GROUP_##name

typedef enum {
    DEFINE_RECORD_ENUM(HA)   = 0, // see record_ha.h/.proto
    DEFINE_RECORD_ENUM(CDB) ,     // see record_cdb.h/.proto
    DEFINE_RECORD_ENUM(ALARM) ,   // see record_alarm.h/.proto
    DEFINE_RECORD_ENUM(MAXNUM)
}RECORDGroup;


#endif    
