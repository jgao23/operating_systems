/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

struct metadata {
	size_t size;
	int free;
	struct metadata *next;
	struct metadata *prev;
};

struct metadata *start = NULL;
struct metadata *last = NULL;

struct metadata *get_block_ptr(void *ptr) {
	return (struct metadata*)(ptr - sizeof(struct metadata));
}

struct metadata *find_block(size_t size) {
	struct metadata *curr = start;
	while (curr && !(curr->free && curr->size >= size)) {
		curr = curr->next;
	}
	return curr;
}

struct metadata *request_space(size_t size) {
	struct metadata *block;
	block = sbrk(0);
	void *valid = sbrk(size + sizeof(struct metadata));
	if (valid == (void*) -1) {
		return NULL; 
	}
	block->size = size;
	block->next = NULL;
	block->free = 1;
	if (last) {
		last->next = block;
		block->prev = last;
		last = last->next;
	} else if (!last) {
		block->prev = NULL;
	}
	return block;
}

void split(struct metadata *block, size_t size) {
	struct metadata *split_block;
	split_block = (void *)block + size + sizeof(struct metadata);
	split_block->size = block->size - size - sizeof(struct metadata);
	split_block->free = 1;
	split_block->next = block->next;
	if (block->next) {
		block->next->prev = split_block;
	}
	block->size = size;
	block->next = split_block;
}

struct metadata *coalesce(struct metadata *curr) {
	struct metadata *next = curr->next;
	curr->size = curr->size + next->size + sizeof(struct metadata);
	curr->next = next->next;
	if (next->next) {
		next->next->prev = curr;
	}
	return curr;
}

void *mm_malloc(size_t size) {
	if (size > 0) {
    	if (!start) {
    		start = request_space(size);
    		if (!start) {
    			return NULL;
    		}
    		start->free = 0;
			last = start;
    		return start+1;
    	} else {
    		struct metadata *block;
    		block = find_block(size);
    		if (!block) {
    			block = request_space(size);
    			if (!block) {
    				return NULL;
    			}	
    		} else {
    			if ((block->size - size) >= (sizeof(struct metadata))) {
    				split(block, size); 
    			}
    		}
    		block->free = 0;
    		void *ptr = (void *)block + sizeof(struct metadata);
    		memset(ptr, 0, size);
    		return ptr;
    	}
    }
    return NULL;
}

void *mm_realloc(void *ptr, size_t size) {
	if (!ptr) {
		return mm_malloc(size);
	} else {
		if (size == 0) {
			return NULL;
		} 
		struct metadata *block = get_block_ptr(ptr);
		mm_free(ptr);
		void *valid = mm_malloc(size);
		if (!valid) {
			return NULL;
		}
		if (size < block->size) {
			memcpy(valid, ptr, size);
		} else {
			memcpy(valid, ptr, block->size);
		}
		return valid;
	}
}

void mm_free(void *ptr) {
	if (ptr != NULL) {
		struct metadata *curr = get_block_ptr(ptr);
		curr->free = 1;
		if (curr->prev && curr->prev->free) {
			curr = coalesce(curr->prev);
		}
		if (curr->next && curr->next->free) {
			coalesce(curr);
		}
	}
}
