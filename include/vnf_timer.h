/*************************************************************************
 Casa Systems Inc.
 Copyright (C) 2015
 All Rights Reserved.

 This file contains proprietary trade secrets of Casa Systems, Inc. No part
 of this file may be reproduced or transmitted in any form or by any means,
 electronic or mechanical, including photocopying and recording, for any
 purpose without the expressed written permission of Casa Systems, Inc.

 File: timer.h

 Author: Wenxing Zheng
 Date:   Sep. 29, 2015

 Header file for generic timer utility.

 Revision History:
*************************************************************************/
#ifndef __VNF_TIMER_H__
#define __VNF_TIMER_H__

#include <stdbool.h>

#define VNF_SECOND (1000)

typedef void *VNFTimer;

/** The prototype of the timer callback function */
typedef void (*VNFTimerCb)(void *);

/** Create a timer object
 *  @param  none
 *  @return timer handle on success, NULL on failure */
VNFTimer vtimer_new(void);

/** Create a timer object
 *  @param  milli_sec  timeout value in milli_seconds
 *  @param  cb the callback function
 *  @param  data the user data passed to the callback
 *               function
 *  @return timer handle on success, NULL on failure */
VNFTimer vtimer_new_full(unsigned int milli_sec, VNFTimerCb cb, void *data);

/** Configure the callback function whihc is invoked upon
 *  timer expiry
 *  @param  timer the handle
 *  @param  cb the callback function
 *  @param  data the user data passed to the callback
 *               function
 *  @return void */
void vtimer_set_callback(VNFTimer timer, VNFTimerCb cb, void *data);

/** Configure the timeout value in milli seconds
 *  @param  timer the handle
 *  @param  milli_sec the timeout value in milli_seconds
 *  @return void */
void vtimer_set_timeout(VNFTimer timer, unsigned int milli_sec);

/** Start/activate the timer in one shot mode. The timer can be
 *  stopped anytime by calling vtimer_stop()
 *  @param  timer the handle
 *  @return true if timer started, false otherwise */
bool vtimer_start(VNFTimer timer);

/** Start/activate the timer in periodic mode. The timer will be
 *  automatically restarted upon expiry. The timer can be
 *  stopped anytime by calling vtimer_stop()
 *  @param  timer the handle
 *  @return true if timer started, false otherwise */
bool vtimer_start_periodic(VNFTimer timer);

/** Check if the timer is running
 *  @param  timer the handle
 *  @return true if the timer is running/ticking, false
 *          otherwise */
bool vtimer_is_running(VNFTimer timer);

/** Stop the timer. The callback will not be invoked after the
 *  timer is stopped
 *  @param  timer the handle
 *  @return void */
void vtimer_stop(VNFTimer timer);

/** Free the timer. Internally, it will stop the timer first
 *  and then free the memory of the timer object
 *  @param  timer the handle
 *  @return void */
void vtimer_free(VNFTimer timer);


#endif

