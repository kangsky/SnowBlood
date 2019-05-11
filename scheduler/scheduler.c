#include "scheduler.h"
#include "stdio.h"
#include "pthread.h"
#include "assert.h"
#include "time.h"
#include "stdlib.h"
#include "string.h"
#include "sb_time.h"
#include "sys/errno.h"

#define DISPATCHER_MAX_TIMEOUT 300000 // 5 minutes
#define DISPATCHER_MAX_HANDLES 1024
#define DISPATCHER_HANDLE_MAP_SIZE (DISPATCHER_MAX_HANDLES/8)+1
#define GET_HANDLE_BIT_VAL(handleMap, index) (handleMap[index/8] >> (index % 8)) & 0x01
#define TOGGLE_HANDLE_BIT_VAL(handleMap, index) handleMap[index/8] ^= 0x01 << (index % 8)

/* Event List, asending order based on absolute time */
typedef struct _event {
	SB_DISPATCHER_HANDLE	handle;
	EventCB 				eventCb;
	void					*arg;
	size_t					argLen;
	struct timespec 		absTime;
	struct _event			*next;
} Event;

/*
	SB scheduler data structure
*/
typedef struct {
	pthread_t 			sb_dispatcher;
	pthread_cond_t		dispatcher_cond;
	pthread_mutex_t		dispatcher_mutex_cond;
	pthread_mutex_t		dispatcher_mutex;
	struct timespec  	next_timeout_timespec;
	Event				*eventList;
	uint8_t				freeHandleMap[DISPATCHER_HANDLE_MAP_SIZE]; // index 0 is reserved for valid check, valid handle value would start from index 1
} SB_Scheduler;


/* Local variables */
SB_Scheduler sb_scheduler;

// Static Local functions
static SB_STATUS addEventToList(EventCB eventCb, void *arg, size_t argLen, uint32_t interval, SB_DISPATCHER_HANDLE* handle);
static SB_STATUS removeEventFromList(SB_DISPATCHER_HANDLE handle);
static SB_STATUS removeHeadOfList(); 
static void *dispatchLoop(void *p);
static void updateNextDispatchedTimeoutInMs(uint32_t timeInMillisecond);
struct timespec getAbsTimeoutWithInterval(uint32_t timeInMillisecond);
static SB_DISPATCHER_HANDLE nextAvaiHandle();
static void updateHandleMap(SB_DISPATCHER_HANDLE handle, bool val);

SB_STATUS initScheduler() {
	int p_result;	

	clock_gettime(CLOCK_REALTIME, &sb_scheduler.next_timeout_timespec);
	updateNextDispatchedTimeoutInMs(DISPATCHER_MAX_TIMEOUT);

	// Init variables
	p_result = pthread_cond_init(&sb_scheduler.dispatcher_cond, NULL);
	if ( p_result != 0 ) {
		return SB_FAIL;
	}

	p_result = pthread_mutex_init(&sb_scheduler.dispatcher_mutex_cond, NULL);
	if ( p_result != 0 ) {
		return SB_FAIL;
	}

	p_result = pthread_mutex_init(&sb_scheduler.dispatcher_mutex, NULL);
	if ( p_result != 0 ) {
		return SB_FAIL;
	}

	memset(&sb_scheduler.freeHandleMap, 0, DISPATCHER_HANDLE_MAP_SIZE);
	
	// Create thread as workloop scheduler
	p_result = pthread_create(&sb_scheduler.sb_dispatcher, NULL, dispatchLoop, NULL);	
	if ( p_result != 0 ) {
		return SB_FAIL;
	}

	/* Init Event List*/
	sb_scheduler.eventList = NULL;
	printf("initScheduler success, thread size  = %zu\n", pthread_get_stacksize_np(sb_scheduler.sb_dispatcher) );
	return SB_OK;
}

SB_STATUS deinitScheduelr() {
	printf("deinitScheduler success!\n");
	return SB_OK;
}

// For first version, let's take 32 bit as timestamp
SB_STATUS dispatchTimedEvent(EventCB eventCb, void *arg, size_t argLen, uint32_t interval, SB_DISPATCHER_HANDLE* handle) {
	SB_STATUS result = SB_OK;	

	// Add event into scheduler eventList 
	result = addEventToList(eventCb, arg, argLen, interval, handle);
	if ( result != SB_OK ) {
		return result;
	}

	return SB_OK;	
}

// Cancel dispatched event
SB_STATUS removeTimedEvent(SB_DISPATCHER_HANDLE handle) {
	return removeEventFromList(handle);
}

/* In general, for a dispatcher, we need to put it into waiting state by pthread_cond_timedwait for a timeout. When timeout occurs, we wake up the thread to process the scheduled event. */
static void *dispatchLoop(void *p) {
	int result = 0;	

	printf("[DEBUG] dispatch loop started\n");
	pthread_setname_np("SB_Dispatcher");

	while(1) {
		pthread_mutex_lock( &sb_scheduler.dispatcher_mutex_cond );
		result = pthread_cond_timedwait( &sb_scheduler.dispatcher_cond, &sb_scheduler.dispatcher_mutex_cond, &sb_scheduler.next_timeout_timespec );
		pthread_mutex_unlock( &sb_scheduler.dispatcher_mutex_cond );
		printf("[DEBUG] condition timedwait result = %d\n", result);
	
		if ( result == ETIMEDOUT && sb_scheduler.eventList ) {
			// if pthread_cond is timed out, tet the head of eventList and execute the event callback
			if ( sb_scheduler.eventList->eventCb ) {
				sb_scheduler.eventList->eventCb( sb_scheduler.eventList->arg, sb_scheduler.eventList->argLen );
			}
				
			// RemoveHeadOfList
			removeHeadOfList();
		} 
		
		// Condition is signaled to update timer, no event is fired at this point : update next timeout and move on here
				
		// Update next event timeout
        pthread_mutex_lock( &sb_scheduler.dispatcher_mutex );
		if ( sb_scheduler.eventList ) {
       		sb_scheduler.next_timeout_timespec.tv_sec = sb_scheduler.eventList->absTime.tv_sec;
        	sb_scheduler.next_timeout_timespec.tv_nsec = sb_scheduler.eventList->absTime.tv_nsec;
        } else {
			// no more events, update next timeout to DEFAULT one
			updateNextDispatchedTimeoutInMs( DISPATCHER_MAX_TIMEOUT );
		}
		pthread_mutex_unlock( &sb_scheduler.dispatcher_mutex );
	}

	return NULL;
}

static void updateNextDispatchedTimeoutInMs(uint32_t timeInMillisecond) {
	clock_gettime(CLOCK_REALTIME, &sb_scheduler.next_timeout_timespec);
	sb_scheduler.next_timeout_timespec.tv_sec += timeInMillisecond / 1000;
	sb_scheduler.next_timeout_timespec.tv_nsec += (timeInMillisecond%1000) * 1000000;
}

struct timespec getAbsTimeoutWithInterval(uint32_t timeInMillisecond) {
	struct timespec absTimeout;

	clock_gettime(CLOCK_REALTIME, &absTimeout);
    absTimeout.tv_sec += timeInMillisecond / 1000;
    absTimeout.tv_nsec += (timeInMillisecond%1000) * 1000000;

	return absTimeout;
}

/* LinkedList for event scheduler */
static SB_STATUS addEventToList(EventCB eventCb, void *arg, size_t argLen, uint32_t interval, SB_DISPATCHER_HANDLE* handle) {
	Event *it = sb_scheduler.eventList;

	printf("[DEBUG] addEventToList callback = %x interval = %d\n", (unsigned int)eventCb, interval);
	Event *newEvent = (Event *)malloc(sizeof(Event));
	if ( !newEvent ) {
		return SB_OUT_OF_MEMORY;
	}

	*handle = nextAvaiHandle(); 
	if ( *handle == SB_DISPATCHER_INVALID_HANDLE ) {
		return SB_OUT_OF_MEMORY; 
	}

	updateHandleMap( *handle, true );
	newEvent->handle = *handle;
	newEvent->eventCb = eventCb;
	newEvent->absTime = getAbsTimeoutWithInterval(interval);
	newEvent->next = NULL;
	// Allocate buffer from Heap for arg during context swtich, release this buffer after event is fired
	newEvent->arg = (void *)malloc(argLen);
	if ( !newEvent->arg ) {
		return SB_OUT_OF_MEMORY;
	}
	newEvent->argLen = argLen;
	memcpy(newEvent->arg, arg, argLen);

	// if head event is NULL OR already larger or euqal to newEvent, insert new node to the head
	if ( !it || SB_compareTimeSpec( it->absTime, newEvent->absTime ) ) {
		newEvent->next = it;
		sb_scheduler.eventList = newEvent;

		printf("[DEBUG] addEventToList update scheduler \n");
		// Signal dispatcher_cond to wake up
		pthread_mutex_lock( &sb_scheduler.dispatcher_mutex_cond );
		pthread_cond_signal( &sb_scheduler.dispatcher_cond );
		pthread_mutex_unlock( &sb_scheduler.dispatcher_mutex_cond );
		
		return SB_OK;
	}

	// Find first even longer than the requseted one
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

static SB_STATUS removeEventFromList(SB_DISPATCHER_HANDLE handle) { 
	if ( handle == SB_DISPATCHER_INVALID_HANDLE ) {
		return SB_INVALID_PARAMETER;
	}

	Event *it = sb_scheduler.eventList;
	Event *temp = NULL;

	// If removed event is the head of list, signal scheduler to update it's next scheduled event
	if ( it && it->handle == handle ) {
		removeHeadOfList();
		printf("[DEBUG] removeEventFromList update scheduler \n");
		// Signal dispatcher_cond to wake up
		pthread_mutex_lock( &sb_scheduler.dispatcher_mutex_cond );
		pthread_cond_signal( &sb_scheduler.dispatcher_cond );
		pthread_mutex_unlock( &sb_scheduler.dispatcher_mutex_cond );
		return SB_OK;	
	}

	// Remove the event from the list
	while ( it->next && it->next->handle != handle) {
		it = it->next;
	}

	if ( it->next ) {
		temp = it->next;
		it->next = it->next->next;
		updateHandleMap( handle, false );
		// free buffer for arg
		free(temp->arg);
		free(temp);		
		return SB_OK;
	} else {
		return SB_INVALID_PARAMETER;
	}
}

static SB_STATUS removeHeadOfList() {
	Event *it = sb_scheduler.eventList;

	updateHandleMap( it->handle, false );
	sb_scheduler.eventList = it->next;
	// free buffer for arg
	free(it->arg);
	free(it);
	it = NULL;

	return SB_OK;
}

static SB_DISPATCHER_HANDLE nextAvaiHandle() {
	uint16_t 	i = 1;
	
	for(; i <  DISPATCHER_MAX_HANDLES ; i++ ) {
		if ( ! ( GET_HANDLE_BIT_VAL(sb_scheduler.freeHandleMap, i) ) ) {
			return i;
		}
	}
	
	return SB_DISPATCHER_INVALID_HANDLE;
}

static void updateHandleMap(SB_DISPATCHER_HANDLE handle, bool val) {
	if ( ! ( (GET_HANDLE_BIT_VAL(sb_scheduler.freeHandleMap, handle)) && val) ) {
		TOGGLE_HANDLE_BIT_VAL(sb_scheduler.freeHandleMap, handle);
	}
}
