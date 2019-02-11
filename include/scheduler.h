#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <event2/event.h>

typedef struct event_base *DBScheduler;
extern __thread DBScheduler db_scheduler;

/** create a scheduler/eventloop for the calling thread
 *  @param  void
 *  @return a scheduler object */
DBScheduler scheduler_init(void);

/** destroy the scheduler
 *  @param  void
 *  @return void */
void scheduler_deinit(void);

/** run teh scheduler/eventloop
 *  @param void
 *  @return 0 on success, error on failure  */
int run_scheduler(void);

/** get the scheduler object of the calling thread */
#define get_scheduler() (db_scheduler)


#endif
