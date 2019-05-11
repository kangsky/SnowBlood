/*
	Heap Memory Management
 */
#include "sb_common.h"

SB_STATUS sb_init_heap(size_t heap_size);

SB_STATUS sb_deinit_heap();

void* sb_malloc(size_t blockLen);

SB_STATUS sb_free(void* ptr);
