#ifndef __DB_API_LOG_H__
#define __DB_API_LOG_H__

#include <sys/types.h>
#include "db_api.h"

#define DB_API_DEFAULT_LOG_LEVEL L_WARNINGS


typedef struct _db_api_events_log {
    LOG_LEVEL log_level;
#define MAX_LOG_STRING_LEN 128
    char      log_str[MAX_LOG_STRING_LEN];
} db_api_events_log;

extern int logging_level_db_api;

#define DB_API_EVENTS_LOG_MAP(XX)                                       \
    /* tag, level, fmt */                                               \
    XX(DB_API_EVENT_SUCCESS, L_DEBUGGING,                               \
       "%s succeeded")                                                  \
    XX(DB_API_EVENT_GENERAL_ERROR, L_ERRORS,                            \
       "Error at %s (line %d) : %d %s (errno = %d)")                    \
    XX(DB_API_EVENT_GENERAL_DEBUG, L_DEBUGGING,                         \
       "Debugging at %s (line %d) : %s (%d)")                           \
    XX(DB_API_EVENT_GENERAL_INFORMATIONAL, L_INFORMATIONAL,             \
       "%s (line %d) : %s (change code = %d)")                          \
    XX(DB_API_EVENT_REPLY_IS, L_DEBUGGING,                              \
       "%s (line %d) : Redis reply is %s, full_read_in_progress=%d, %d elements") \
    XX(DB_API_EVENT_EXEC_CMD, L_DEBUGGING,                              \
       "%s (line %d) : exec DB cmd: %s")                                \
    XX(DB_API_EVENT_EXEC_CMD_RET, L_INFORMATIONAL,                      \
       "%s (line %d) : exec DB cmd [%s] ret is %d and ctx error is  %s")     \
    XX(DB_API_EVENT_RECV_CMD_CB, L_DEBUGGING,                           \
       "%s (line %d) : Receive callback for cmd (%s)")                  \
    XX(DB_API_EVENT_REPEAT_CMD, L_DEBUGGING,                            \
       "%s (line %d) : Repeat DB CMD %s (ret=%d)")                      \
    XX(DB_API_EVENT_REDIS_CTX, L_DEBUGGING,                             \
       "%s (line %d) : %s (%p)")                                        \
    XX(DB_API_EVENT_SELECT_DB, L_DEBUGGING,                             \
       "%s (line %d) : select db %d")                                   \
    XX(DB_API_EVENT_DB_ERROR, L_ERRORS,                                 \
       "%s (line %d) : db error (%s)")                                  \
    XX(DB_API_EVENT_REPLY_ERROR, L_ERRORS,                              \
       "%s (line %d) : db reply error (%s), full_read_in_progress=%d, %d elements") \
    XX(DB_API_EVENT_UNKNOWN, L_ERRORS,                                  \
       "Unknown %s (line %d) : %d")                                     \

    // end of map

typedef enum {
#define XX(tag, level, fmt) tag,
DB_API_EVENTS_LOG_MAP(XX)
#undef XX
    DB_API_EVENT_MAX
} db_api_events;

extern const db_api_events_log DB_API_EVENTS[DB_API_EVENT_MAX];

#define DB_API_LOG(event_n, args...)                                \
{                                                                       \
    if (((event_n) < DB_API_EVENT_MAX) &&                           \
        (logging_level_db_api >= DB_API_EVENTS[event_n].log_level)) { \
        CASALOG(L_DBAPI, DB_API_EVENTS[event_n].log_level,          \
                event_n, DB_API_EVENTS[event_n].log_str, args); \
    }                                                                   \
} 

bool db_api_event_logging_capable(uint32_t event_n);
#endif
