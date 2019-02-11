#include "scheduler.h"

__thread DBScheduler db_scheduler;

/* Scheduler is a singleton object */
DBScheduler scheduler_init(void)
{
    if ( db_scheduler == NULL) {
        db_scheduler = event_base_new();
    }
    return db_scheduler;

}


/* Run the event loop */
int run_scheduler(void)
{
    int rc = -1;

    if ( db_scheduler ) {
         rc = event_base_dispatch( db_scheduler );
    }

    return rc;
}

/* Shut down the event loop */
void scheduler_deinit(void)
{
    if ( db_scheduler ) {
        event_base_free( db_scheduler );
        db_scheduler = NULL;
    }

}
