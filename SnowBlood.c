#include "stdio.h"
#include "stdlib.h"
#include "scheduler.h"
#include "memory.h"
#include "time.h"
#include "pthread.h"
#include "assert.h"

#define SB_SCHEDULER_DEBUG 0
#define SB_MEMORY_DEBUG 1

#define DUMMY_TIMEOUT_1 10000
#define DUMMY_TIMEOUT_2 15000
#define DUMMY_TIMEOUT_3 5000

static void dummyEvent1(void *arg, size_t argLen);
static void dummyEvent2(void *arg, size_t argLen);
static void dummyEvent3(void *arg, size_t argLen);

static struct timespec current_ts;

SB_DISPATCHER_HANDLE dummy_handle_1, dummy_handle_2, dummy_handle_3;


#define HEAP_SIZE 1024*1024 // 1 MB

int main(int argc, char *argv[]) {
	SB_STATUS	sb_result;

#if SB_MEMORY_DEBUG
	sb_result = sb_init_memory(HEAP_SIZE, SB_MEM_CONFIG_MODE_1);
	assert (sb_result == SB_OK);
	
	uint8_t *ptr_1 = NULL;
	uint32_t *ptr_2 = NULL;
	
	ptr_1 = (uint8_t *)sb_malloc(5*sizeof(uint8_t));
	assert (ptr_1);
	ptr_2 = (uint32_t*)sb_malloc(2*sizeof(uint32_t))	
	assert (ptr_2);
	printf("memory debug : ptr_1 = 0x%x ptr_2= 0x%x\n", ptr_1, ptr_2);
#endif


#if SB_SCHEDULER_DEBUG
	uint8_t arg_1 = 0x14;
	uint32_t arg_2 = 0x12345678;
	char arg_3[5] = {'a', 'b', 'c', 'd', 'e'};

	clock_gettime(CLOCK_REALTIME, &current_ts); 
	printf("Snow Blood! Current second = %ld, timeout_1 = %d ms, timeout_2 = %d ms, tiemout_3 = %d ms \n", current_ts.tv_sec, DUMMY_TIMEOUT_1, DUMMY_TIMEOUT_2, DUMMY_TIMEOUT_3);

	/* Init scheduler */
	sb_result = initScheduler();

	/* Yield for scheduler for init */
	pthread_yield_np();

	/* Schedule a dummy Event */
	sb_result = dispatchTimedEvent(dummyEvent1, &arg_1, sizeof(arg_1), DUMMY_TIMEOUT_1, &dummy_handle_1);

	sb_result = dispatchTimedEvent(dummyEvent2, &arg_2, sizeof(arg_2), DUMMY_TIMEOUT_2, &dummy_handle_2);

	sb_result = dispatchTimedEvent(dummyEvent3, arg_3, sizeof(arg_3), DUMMY_TIMEOUT_3, &dummy_handle_3);
	
	printf("handle value = %d %d %d \n", dummy_handle_1, dummy_handle_2, dummy_handle_3);
	
	while(1);
#endif

	return 0;
}


static void dummyEvent1(void *arg, size_t argLen) {
	clock_gettime(CLOCK_REALTIME, &current_ts);
	assert( argLen == sizeof(uint8_t) );
	printf("DummyEvent1, current second = %ld, arg = %d\n", current_ts.tv_sec, *((uint8_t *)arg) );
}

static void dummyEvent2(void *arg, size_t argLen) {
	clock_gettime(CLOCK_REALTIME, &current_ts); 
	assert( argLen == sizeof(uint32_t) );
	printf("DummyEvent2, current second = %ld, arg = %d\n", current_ts.tv_sec, *((uint32_t *)arg) );
}

static void dummyEvent3(void *arg, size_t argLen) {
	clock_gettime(CLOCK_REALTIME, &current_ts); 
	assert( argLen == 5 );
	dispatchTimedEvent(dummyEvent3, arg, argLen, DUMMY_TIMEOUT_3, &dummy_handle_3);
	printf("DummyEvent3, current second = %ld, handle_3 = %d\n", current_ts.tv_sec, dummy_handle_3);
	
	char *c = (char *)arg;
	uint8_t i = 0;

	while(i<argLen) {
		printf("%c ", *(c++) );
		i++;
	}
	printf("\n");
}
