#include "stdio.h"
#include "stdlib.h"
#include "scheduler.h"
#include "time.h"
#include "pthread.h"

#define DUMMY_TIMEOUT_1 10000
#define DUMMY_TIMEOUT_2 15000
#define DUMMY_TIMEOUT_3 5000

static void dummyEvent1(void *argv);
static void dummyEvent2(void *argv);
static void dummyEvent3(void *argv);

static struct timespec current_ts;

SB_DISPATCHER_HANDLE dummy_handle_1, dummy_handle_2, dummy_handle_3;

int main(int argc, char *argv[]) {
	SB_STATUS	sb_result;


	clock_gettime(CLOCK_REALTIME, &current_ts); 
	printf("Snow Blood! Current second = %ld, timeout_1 = %d ms, timeout_2 = %d ms, tiemout_3 = %d ms \n", current_ts.tv_sec, DUMMY_TIMEOUT_1, DUMMY_TIMEOUT_2, DUMMY_TIMEOUT_3);

	/* Init scheduler */
	sb_result = initScheduler();

	/* Yield for scheduler for init */
	pthread_yield_np();

	/* Schedule a dummy Event */
	sb_result = dispatchTimedEvent(dummyEvent1, NULL, DUMMY_TIMEOUT_1, &dummy_handle_1);

	sb_result = dispatchTimedEvent(dummyEvent2, NULL, DUMMY_TIMEOUT_2, &dummy_handle_2);

	sb_result = dispatchTimedEvent(dummyEvent3, NULL, DUMMY_TIMEOUT_3, &dummy_handle_3);
	
	printf("handle value = %d %d %d \n", dummy_handle_1, dummy_handle_2, dummy_handle_3);
	
	while(1);
	return 0;
}


static void dummyEvent1(void *argv) {
	clock_gettime(CLOCK_REALTIME, &current_ts); 
	printf("DummyEvent1, current second = %ld\n", current_ts.tv_sec);
}

static void dummyEvent2(void *argv) {
	clock_gettime(CLOCK_REALTIME, &current_ts); 
	printf("DummyEvent2, current second = %ld\n", current_ts.tv_sec);
}

static void dummyEvent3(void *argv) {
	clock_gettime(CLOCK_REALTIME, &current_ts); 
	dispatchTimedEvent(dummyEvent3, NULL, DUMMY_TIMEOUT_3, &dummy_handle_3);
	printf("DummyEvent3, current second = %ld, handle_3 = %d\n", current_ts.tv_sec, dummy_handle_3);
}
