#include "vnf_timer.h"
#include "scheduler.h"
#include <glib.h>
#include <event2/event_struct.h>


typedef struct {
    VNFTimerCb     cb;
    void           *data;
    struct timeval time_out;
    uint32_t       periodic:1;
    uint32_t       initialized:1;
    uint32_t       running:1;
    struct event   timer_event;
}VNFTimerImpl;

static void timer_event_cb(evutil_socket_t fd, short event, void *arg);


static void timer_event_cb(evutil_socket_t fd, short event, void *arg)
{
    VNFTimerImpl *tmr = arg;

    if (!tmr) 
        return;

    tmr->running = false;
    if (tmr->cb) {
        tmr->cb(tmr->data);
        if (tmr->periodic) {
            // start periodic timer automatically
            evtimer_add(&tmr->timer_event, &tmr->time_out);
            tmr->running =true;
        }
    }
}

VNFTimer vtimer_new(void)
{
    VNFTimerImpl *tmr = g_slice_new0(VNFTimerImpl);

    if ( tmr ) {
        evtimer_assign(&tmr->timer_event, get_scheduler(), timer_event_cb, tmr);
    }

    return (VNFTimer)tmr;
}

VNFTimer vtimer_new_full(unsigned int msec, VNFTimerCb cb, void *data)
{
    VNFTimerImpl *tmr = g_slice_new0(VNFTimerImpl);

    if ( tmr ) {
        evtimer_assign(&tmr->timer_event, get_scheduler(), timer_event_cb, tmr);
        tmr->cb = cb;
        tmr->data =data;
        tmr->time_out.tv_sec = msec/1000;
        tmr->time_out.tv_usec = (msec % 1000) * 1000;
        tmr->initialized =  true;
    }

    return (VNFTimer)tmr;

}

bool vtimer_start(VNFTimer timer)
{
    VNFTimerImpl *tmr = timer;
    if ( !tmr || !tmr->initialized ) {
        return false;
    }

    if ( evtimer_add(&tmr->timer_event, &tmr->time_out) == 0 ) {
        tmr->running = true;
        return true;
    }

    return false;
}


bool vtimer_start_periodic(VNFTimer timer)
{
    VNFTimerImpl *tmr = timer;

    if ( vtimer_start(tmr) ) {
        tmr->periodic = true;
        return true;
    }

    return false;
}

bool vtimer_is_running(VNFTimer timer)
{
    VNFTimerImpl *tmr = timer;

    return tmr ? tmr->running : false; 
}

void vtimer_stop(VNFTimer timer)
{
    VNFTimerImpl *tmr = timer;

    if( !tmr ) {
        return ;
    }
    tmr->periodic = false;
    tmr->running = false;
    evtimer_del(&tmr->timer_event);

}


void vtimer_free(VNFTimer timer)
{
    vtimer_stop(timer);
    g_slice_free(VNFTimerImpl, timer);

}

void  vtimer_set_callback(VNFTimer timer, VNFTimerCb cb, void *data)
{
    VNFTimerImpl *tmr = timer;

    if (!tmr) {
        return;
    }

    tmr->cb = cb;
    tmr->data =data;

    return;


}

void vtimer_set_timeout(VNFTimer timer, unsigned int msec)
{

    VNFTimerImpl *tmr = timer;

    if ( !tmr ) {
        return;
    }

    tmr->time_out.tv_sec = msec/1000;
    tmr->time_out.tv_usec = (msec % 1000) * 1000;
    tmr->initialized =  true;

    return;
}

