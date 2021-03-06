/*
	Scheduler
		Function dispatcher with specified timer
 */
#ifndef SB_SCHEDULER_H
#define SB_SCHEDULER_H

#include "sb_common.h"

#define SB_DISPATCHER_INVALID_HANDLE 0
typedef uint16_t SB_DISPATCHER_HANDLE;
typedef void (*EventCB)(void *arg, size_t argLen);

SB_STATUS initScheduler();

SB_STATUS deinitScheduelr();

// For first version, let's take 32 bit as timestamp, unit is millisecond
SB_STATUS dispatchTimedEvent(EventCB eventCb, void *arg, size_t argLen, uint32_t interval, SB_DISPATCHER_HANDLE* handle);

// Cancel dispatched event
SB_STATUS removeTimedEvent(SB_DISPATCHER_HANDLE handle);

#endif
