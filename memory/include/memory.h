/*
	Heap Memory Management
 */
#ifndef SB_MEMORY_H
#define SB_MEMORY_H 

#include "sb_common.h"

typedef enum {
	SB_MEM_CONFIG_MODE_0 = 0x00, // Mode 1 : Pre-allocate a set of memory block with fixed size
	SB_MEM_CONFIG_MODE_1 = 0x01, // Mode 2 : Dynamically maintain the heap with double linked list
} SB_MEM_CONFIG_MODE;

SB_STATUS sb_init_memory(size_t heap_size, SB_MEM_CONFIG_MODE mem_cfg_mode);

SB_STATUS sb_deinit_memory();

void* sb_malloc(size_t block_len);

SB_STATUS sb_free(void* ptr);

#endif
