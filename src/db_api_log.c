
#include "db_api_log.h"

const db_api_events_log DB_API_EVENTS[DB_API_EVENT_MAX] = {
#define XX(tag, level, fmt) {level, fmt},
DB_API_EVENTS_LOG_MAP(XX)
#undef XX
};

bool db_api_event_logging_capable(uint32_t event_n)
{
    return ((event_n < DB_API_EVENT_MAX) &&
            (logging_level_db_api >= DB_API_EVENTS[event_n].log_level));
}

