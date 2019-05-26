#include "memory.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"


/*
	Memory Management for Heap
 */

/*
	Memory Block Format
	HEADER | PADDING BYTES | USER MEMORY BLOCK 
 */
typedef struct _block_entry {
	uint8_t *blk_addr;
	size_t	blk_size;
	size_t	next_fragment_size; // fragment size between current block and next block
	struct _block_entry *pre_blk;
	struct _block_entry	*next_blk;
} mem_blk_list;

#define BLK_HEADER sizeof(mem_blk_list)
#define ALIGN_SIZE 8 // word size for x86_64 architecture
#define ALIGN_BIT_SHIFT 3
#define ALIGN_BIT_MASK 0x0000000000000007

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
	mem_blk_list *new_blk = NULL;
	size_t full_blk_size = block_size + BLK_HEADER + (ALIGN_SIZE - 1) ;

	if ( !blk ) {
		// blk_list is empty
		new_blk = (mem_blk_list *)heap_ctx.heap;
	
		new_blk->blk_size = full_blk_size;
		new_blk->pre_blk = NULL;
		new_blk->next_blk = NULL;

		if ( full_blk_size > heap_ctx.heap_size ) {
			// Out of memory
			return NULL;
		}

		// Update blk_addr and next_fragment_size
		new_blk->blk_addr = (uint8_t *)new_blk + BLK_HEADER;
		if ( (size_t)new_blk->blk_addr & ALIGN_BIT_MASK ) {
			// alignment adjustment
			new_blk->blk_addr = (uint8_t *) ((((size_t)new_blk->blk_addr >> ALIGN_BIT_SHIFT) + 0x01) << ALIGN_BIT_SHIFT );
		}

		new_blk->next_fragment_size = heap_ctx.heap_size - full_blk_size;
		
		blk_list = new_blk;	
	
	} else {
		// blk_list is not emtry, update the list based on request
		while ( blk && blk->next_fragment_size < full_blk_size ) {
			// Find fist available fragment which can meet the request block size
			blk = blk->next_blk;
		}
		if ( !blk ) {
			return NULL;
		}

		// Update new_blk addr
		new_blk = (mem_blk_list *)( (uint8_t *)blk + blk->blk_size );
		
		// Update current new block info and its linking relation
		new_blk->blk_addr = (uint8_t *)new_blk + BLK_HEADER;
		if ( (size_t)new_blk->blk_addr & ALIGN_BIT_MASK ) {
			// alignment adjustment
			new_blk->blk_addr = (uint8_t *) ((((size_t)new_blk->blk_addr >> ALIGN_BIT_SHIFT) + 0x01) << ALIGN_BIT_SHIFT );
		}

		new_blk->blk_size = full_blk_size;
		new_blk->next_fragment_size = blk->next_fragment_size - new_blk->blk_size;
		new_blk->pre_blk = blk;
		new_blk->next_blk = blk->next_blk;
	
		// Update next block linking relation
		if ( blk->next_blk ) {
			blk->next_blk->pre_blk = new_blk;
		}
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

	if ( blk == blk_list ) {
		blk_list = blk->next_blk;
	}

	// Update next block linking relation
	if ( blk->next_blk ) {
		blk->next_blk->pre_blk = blk->pre_blk;
	}

	// Update previous block info and its linking relation
	if ( blk->pre_blk ) {
		blk->pre_blk->next_fragment_size += blk->blk_size;
		// If current blk is the end block in the list, the next_fragment_size needs to be added back in previous block
		blk->pre_blk->next_fragment_size += blk->next_blk ? 0 : blk->next_fragment_size;
		blk->pre_blk->next_blk = blk->next_blk;
	}

	// Free current block info : memory reset, free block entry etc
	memset(blk, 0, blk->blk_size);
	
	return SB_OK;
}
