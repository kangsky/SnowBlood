/*
	Scheduler
		Function dispatcher with specified timer
 */
#include "sb_common.h"

typedef void (*EventCB)(void *argv);

SB_STATUS initScheduler();

SB_STATUS deinitScheduelr();

// For first version, let's take 32 bit as timestamp, unit is millisecond
SB_STATUS dispatchTimedFunction(EventCB eventCb, void *argv, uint32_t interval);
