#include "scheduler.h"
#include "stdio.h"
#include "pthread.h"
#include "assert.h"
#include "time.h"
#include "stdlib.h"
#include "sb_time.h"
#include "sys/errno.h"

/*
	TODO list :
		1. Group next_timeout_timespec + EventList
		2. Add API to cancel scheduled event in dispatcher 
 */

#define DISPATCHER_MAX_TIMEOUT 300000 // 5 minutes

/* Event List, asending order based on absolute time */
typedef struct _event {
	EventCB 			eventCb;
	void				*argv;
	struct timespec 	absTime;
	struct _event		*next;
} Event;

/*
	Group next_timeout_timespec + EventList
*/

/* Local variables */
// pthread for dispatcher
static pthread_t 		sb_dispatcher;
static pthread_cond_t 	dispatcher_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t	dispatcher_mutex_cond = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	dispatcher_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct timespec	next_timeout_timespec;
static Event			*eventList;

// Static Local functions
static SB_STATUS addEventToList(EventCB eventCb, void *argv, uint32_t interval);
static SB_STATUS removeHeadOfList(); 
static void *dispatchLoop(void *p);
static void updateNextDispatchedTimeoutInMs(uint32_t timeInMillisecond);
struct timespec getAbsTimeoutWithInterval(uint32_t timeInMillisecond);


SB_STATUS initScheduler() {
	int p_result;	

	clock_gettime(CLOCK_REALTIME, &next_timeout_timespec);
	updateNextDispatchedTimeoutInMs(DISPATCHER_MAX_TIMEOUT);
	
	// Create thread as workloop scheduler
	p_result = pthread_create(&sb_dispatcher, NULL, dispatchLoop, NULL);	
	if ( p_result != 0 ) {
		return SB_FAIL;
	}

	/* Init Event List*/
	eventList = NULL;
	printf("initScheduler success, thread size  = %zu\n", pthread_get_stacksize_np(sb_dispatcher) );
	return SB_OK;
}

SB_STATUS deinitScheduelr() {
	printf("deinitScheduler success!\n");
	return SB_OK;
}

// For first version, let's take 32 bit as timestamp
SB_STATUS dispatchTimedFunction(EventCB eventCb, void *argv, uint32_t interval) {
	SB_STATUS result = SB_OK;	

	// Add event into scheduler eventList 
	result = addEventToList(eventCb, argv, interval);
	if ( result != SB_OK ) {
		return result;
	}

	return SB_OK;	
}

/* In general, for a dispatcher, we need to put it into waiting state by pthread_cond_timedwait for a timeout. When timeout occurs, we wake up the thread to process the scheduled event. */
static void *dispatchLoop(void *p) {
	int result = 0;	

	printf("[DEBUG] dispatch loop started\n");
	pthread_setname_np("SB_Dispatcher");

	while(1) {
		pthread_mutex_lock( &dispatcher_mutex_cond );
		result = pthread_cond_timedwait( &dispatcher_cond, &dispatcher_mutex_cond, &next_timeout_timespec );
		pthread_mutex_unlock( &dispatcher_mutex_cond );
		printf("[DEBUG] condition timedwait result = %d\n", result);
	
		if ( result == ETIMEDOUT && eventList ) {
			// if pthread_cond is timed out, tet the head of eventList and execute the event callback
			if ( eventList->eventCb ) {
				eventList->eventCb(NULL);
			}
				
			// RemoveHeadOfList
			removeHeadOfList();
		} 
		
		// Condition is signaled to update timer, no event is fired at this point : update next timeout and move on here
				
		// Update next event timeout
        pthread_mutex_lock( &dispatcher_mutex );
		if ( eventList ) {
       		next_timeout_timespec.tv_sec = eventList->absTime.tv_sec;
        	next_timeout_timespec.tv_nsec = eventList->absTime.tv_nsec;
        } else {
			// no more events, update next timeout to DEFAULT one
			updateNextDispatchedTimeoutInMs( DISPATCHER_MAX_TIMEOUT );
		}
		pthread_mutex_unlock( &dispatcher_mutex );
	}

	return NULL;
}

static void updateNextDispatchedTimeoutInMs(uint32_t timeInMillisecond) {
	clock_gettime(CLOCK_REALTIME, &next_timeout_timespec);
	next_timeout_timespec.tv_sec += timeInMillisecond / 1000;
	next_timeout_timespec.tv_nsec += (timeInMillisecond%1000) * 1000000;
}

struct timespec getAbsTimeoutWithInterval(uint32_t timeInMillisecond) {
	struct timespec absTimeout;

	clock_gettime(CLOCK_REALTIME, &absTimeout);
    absTimeout.tv_sec += timeInMillisecond / 1000;
    absTimeout.tv_nsec += (timeInMillisecond%1000) * 1000000;

	return absTimeout;
}

/* LinkedList for event scheduler */
static SB_STATUS addEventToList(EventCB eventCb, void *argv, uint32_t interval) {
	Event *it = eventList;

	printf("[DEBUG] addEventToList callback = %x interval = %d\n", (unsigned int)eventCb, interval);
	Event *newEvent = (Event *)malloc(sizeof(Event));
	if ( !newEvent ) {
		return SB_OUT_OF_MEMORY;
	}
	
	newEvent->eventCb = eventCb;
	newEvent->argv = argv;
	newEvent->absTime = getAbsTimeoutWithInterval(interval);
	newEvent->next = NULL;

	// if head event is NULL OR already larger or euqal to newEvent, insert new node to the head
	if ( !it || SB_compareTimeSpec( it->absTime, newEvent->absTime ) ) {
		newEvent->next = it;
		eventList = newEvent;

		printf("[DEBUG] addEventToList update scheduler \n");
		// Signal dispatcher_cond to wake up
		pthread_mutex_lock( &dispatcher_mutex_cond );
		pthread_cond_signal( &dispatcher_cond );
		pthread_mutex_unlock( &dispatcher_mutex_cond );
		
		return SB_OK;
	}

	while ( it && it->next ) {
		if ( SB_compareTimeSpec( it->next->absTime, newEvent->absTime ) ) {
			break;
		}
		it = it->next;
	}

	// Either it->next is NULL or it->next->absTime is larger than or equal to newEvent->absTime
	newEvent->next = it->next;
	it->next = newEvent;
	
	return SB_OK;
}

static SB_STATUS removeHeadOfList() {
	Event *it = eventList;

	eventList = it->next;
	free(it);
	it = NULL;

	return SB_OK;
}
