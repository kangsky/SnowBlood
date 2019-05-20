#include "memory.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"

/*
	Memory Management for Heap
 */
typedef struct _block_entry {
	uint8_t *blk_addr;
	size_t	blk_size;
	size_t	next_fragment_size; // fragment size between current block and next block
	struct _block_entry *pre_blk;
	struct _block_entry	*next_blk;
} mem_blk_list;

typedef struct {
	uint8_t *heap;
	size_t heap_size;	
} heap_context;

heap_context heap_ctx;
mem_blk_list *blk_list;

SB_STATUS sb_init_memory(size_t heap_size, SB_MEM_CONFIG_MODE mem_cfg_mode) {
	
	heap_ctx.heap = (uint8_t *)malloc(sizeof(uint8_t)*heap_size);
	if ( !heap_ctx.heap ) {
		return SB_OUT_OF_MEMORY;
	}
	
	heap_ctx.heap_size = heap_size;
	memset(heap_ctx.heap, 0, heap_size);

	if ( mem_cfg_mode == SB_MEM_CONFIG_MODE_0 ) {
		return SB_NOT_SUPPORTED;
	} else if ( mem_cfg_mode == SB_MEM_CONFIG_MODE_1 ) {
		blk_list = NULL;
	} else {
		return SB_INVALID_PARAMETER;
	}
	
	return SB_OK;
}

SB_STATUS sb_deinit_memory() {
	if ( heap_ctx.heap ) {
		free( heap_ctx.heap );
	}
	return SB_OK;
}

void* sb_malloc(size_t block_size) {
	mem_blk_list *blk = blk_list;
	
	mem_blk_list *new_blk = (mem_blk_list *)malloc( sizeof(mem_blk_list) );
	if ( !new_blk ) {
		return NULL;
	}
	new_blk->blk_addr = NULL;
	new_blk->blk_size = block_size;
	new_blk->next_fragment_size = 0;
	new_blk->pre_blk = NULL;
	new_blk->next_blk = NULL;


	if ( !blk ) {
		// blk_list is empty
		if ( block_size > heap_ctx.heap_size ) {
			// Out of memory
			free(new_blk);
			return NULL;
		}

		// Update blk_addr and next_fragment_size
		new_blk->blk_addr = heap_ctx.heap;
		new_blk->next_fragment_size = heap_ctx.heap_size - block_size;
		
		blk_list = new_blk;	
	
	} else {
		// blk_list is not emtry, update the list based on request
		while ( blk && blk->next_fragment_size < block_size ) {
			// Find fist available fragment which can meet the request block size
			blk = blk->next_blk;
		}
		if ( !blk ) {
			free(new_blk);
			return NULL;
		}
		
		// Update current new block info and its linking relation
		new_blk->blk_addr = blk->blk_addr + blk->blk_size;
		new_blk->next_fragment_size = blk->next_fragment_size - new_blk->blk_size;
		new_blk->pre_blk = blk;
		new_blk->next_blk = blk->next_blk;
	
		// Update next block linking relation
		blk->next_blk->pre_blk = new_blk;
		
		// Update previous block info and its linking relation
		blk->next_blk = new_blk;
		blk->next_fragment_size = 0;
	}

	return new_blk->blk_addr;
}

SB_STATUS sb_free(void* ptr) {
	mem_blk_list *blk = blk_list;	

	assert( ptr );
	
	// Find the memory block
	while( blk && blk->blk_addr != ptr ) {
		blk = blk->next_blk;
	}
	
	assert( blk );

	// Update next block linking relation
	blk->next_blk->pre_blk = blk->pre_blk;

	// Update previous block info and its linking relation
	blk->pre_blk->next_fragment_size += blk->blk_size;
	blk->pre_blk->next_blk = blk->next_blk;

	// Free current block info : memory reset, free block entry etc
	memset(blk->blk_addr, 0, blk->blk_size);
	free(blk);
	
	return SB_OK;
}
