#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdlib.h>
#include "mem.h"

#define HEADER (32)
#define SLACK (32)

struct node_block {
	struct node_block *next;
	struct node_block *prev;
	int size;
};

struct node_block *free_block_head = NULL;
int m_error;

int Mem_Init(int sizeOfRegion) {
	
	// check for invalid sizeOfRegion or multiple mem init call
	if(sizeOfRegion <= 0 ||  free_block_head != NULL) {
		m_error = E_BAD_ARGS;
		return -1;
	}
	
	// get page size
	int pageSize = getpagesize();
	
	// round up pageSize if necessary
	if(sizeOfRegion % pageSize != 0) {
		sizeOfRegion += pageSize - (sizeOfRegion % pageSize);
	}
	
	int fd = open("/dev/zero", O_RDWR);
	
	void *ptr = mmap(NULL, sizeOfRegion, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	
	if(ptr == MAP_FAILED) { 
		perror("mmap"); 
		m_error = E_BAD_ARGS;
		return -1; 
	}
	
	// create a free list with a block that points to itself
	free_block_head = (struct node_block *) ptr;
	free_block_head->next = NULL; 
	free_block_head->prev = NULL;
	
	// size only refer to space available, exclude header/node_block size
	free_block_head->size = sizeOfRegion - sizeof(struct node_block);
	
	// close the device
	close(fd);
	
	return 0;
	
} // END OF MEM_INIT

void *Mem_Alloc(int size) {
	struct node_block *cur_block = free_block_head;
	struct node_block *prev_block = NULL;
	struct node_block *best_block = NULL;
	
	// do something to calculate real size
	int real_size = sizeof(struct node_block) + size;
	
	if(real_size % 8 != 0) {
		real_size +=  8 - (size % 8);
	}
	
	// go through free_block_head
	while(cur_block != NULL) {
	
		if(cur_block->size + sizeof(struct node_block) == real_size) {
			best_block = cur_block; // this fit nicely, grab and go
			break;
		} else if(cur_block->size + sizeof(struct node_block) > real_size) {
		
			// check whether this is best than current best block
			if(best_block == NULL) {
				best_block = cur_block; // no best block yet, just assign!
			} else if(cur_block->size < best_block->size) {
				best_block = cur_block;
			}
		}
			
		// proceed to next block
		cur_block = cur_block->next;
	} 
	
	if(best_block == NULL) { // no suitable block available!
		m_error = E_NO_SPACE;
		return NULL;
	}
	
	if(best_block == free_block_head) { 
		free_block_head->prev = NULL;
		// if nice fit, just update the header
		if(best_block->size + sizeof(struct node_block) == real_size) {
			free_block_head = best_block->next;
		} else {
		
			// still need to check whether there is enough space for header after we cut the block
			if(best_block->size + sizeof(struct node_block) - real_size < sizeof(struct node_block)) {
				free_block_head = best_block->next;			
			} else {
				best_block->size = best_block->size - real_size; // update the block size
				best_block = sizeof(struct node_block) + (void *) best_block + best_block->size; // point to header of new allocated space
				best_block->size = real_size - sizeof(struct node_block); // set the size of newly allocated space
			}
		}
	} else { // if block is not the head
	
		// if nice fit, update the link
		if(best_block->size + sizeof(struct node_block) == real_size) {
			prev_block = best_block->prev;
			prev_block->next = best_block->next; // remove best block from the link
			if(best_block->next != NULL) best_block->next->prev = prev_block;
		} else {
		
			// still need to check whether there is enough space for header after we cut the block
			if(best_block->size + sizeof(struct node_block) - real_size < sizeof(struct node_block)) {
				prev_block = best_block->prev;
				prev_block->next = best_block->next;			
				if(best_block->next != NULL) best_block->next->prev = prev_block;
			} else {
				best_block->size = best_block->size - real_size; // update the block size
				best_block = sizeof(struct node_block) + (void *) best_block + best_block->size; // point to header of new allocated space
				best_block->size = real_size - sizeof(struct node_block); // set the size of newly allocated space		
			}
		}
	}
	
	return ((void *) best_block + sizeof(struct node_block));
	
} // END OF MEM_ALLOC

int Mem_Free(void *ptr, int coalesce) {
	
	if(ptr == NULL) {
		return 0;
	}
	
	//void * new_block_ptr = ptr - sizeof(struct node_block);
	struct node_block *ptr_header;
	struct node_block *cur_block;
	//struct node_block *prev = free_block_head;
	
	// point to block header
	ptr_header = (struct node_block*) (ptr - sizeof(struct node_block));
	
	// cur block points to free list header
	cur_block = free_block_head;
	
	// if the allocated memory comes before the free list
	if( (uintptr_t *) ptr_header < (uintptr_t *) cur_block ) {
		free_block_head->prev = ptr_header;
		ptr_header->next = free_block_head;
		ptr_header->prev = NULL;
		free_block_head = ptr_header;
	} else { // comes after free list
		
		while(cur_block->next != NULL) {
			if( (uintptr_t *) cur_block->next > (uintptr_t *) ptr_header ) {
				break;
			}
			cur_block = cur_block->next;
		}
		
		if(cur_block == free_block_head) { // in front of list
			ptr_header->prev = cur_block;
			ptr_header->next = cur_block->next;
			if(ptr_header->next != NULL) ptr_header->next->prev = ptr_header;
			cur_block->next = ptr_header;
		} else if (cur_block->next == NULL) { // end of list
			ptr_header->prev = cur_block;
			if(ptr_header->next != NULL) ptr_header->prev->next = ptr_header;
			ptr_header->next = cur_block->next;
			cur_block->next = ptr_header;
		} else if (cur_block != free_block_head) { // middle
			ptr_header->prev = cur_block;
			ptr_header->next = cur_block->next;
			cur_block->next = ptr_header;
			if(ptr_header->next != NULL) ptr_header->next->prev = ptr_header;
		}
		
	}
	
	// check whether need to coalesce
	if(coalesce != 0) {
		//cur_block = ptr_header;
		cur_block = free_block_head;
		
		/*
		// check for left coalesce
		if( (ptr_header->prev != NULL) && ((void *) ptr_header->prev == (void *) ptr_header - ptr_header->prev->size - sizeof(struct node_block)) ) {
			ptr_header->prev->size += (ptr_header->size + sizeof(struct node_block)); // update size
			ptr_header->prev->next = ptr_header->next;
			if(ptr_header->next != NULL) ptr_header->next->prev = ptr_header->prev;
			ptr_header = ptr_header->prev;
		}
		
		
		// check for right coalesce
		if( ptr_header->next != NULL && (void *) ptr_header->next == (void *) ptr_header + ptr_header->next->size + sizeof(struct node_block)) {
			ptr_header->size += (ptr_header->next->size + sizeof(struct node_block)); // update size
			if(ptr_header->next->next != NULL) ptr_header->next->next->prev = ptr_header;
			ptr_header->next = ptr_header->next->next; // update the next
		}
		*/
		
		while(cur_block != NULL && cur_block->next != NULL) {
			if((void *) cur_block + sizeof(struct node_block) + cur_block->size == (void *) cur_block->next) {
			
				// update the size to include the adjacent block and header
				cur_block->size += (cur_block->next->size + sizeof(struct node_block));
				
				// remove the adjacent block from the link
				cur_block->next = cur_block->next->next;
			} else {
				cur_block = cur_block->next;
			}
		}
		
		
	}
	
	return 0;
	
} // END OF MEM_FREE

void Mem_Dump() {

	struct node_block *cur_block = free_block_head;
	
	while(cur_block != NULL) {
		printf("Cur block: %p , Prev block: %p, Next block: %p, Size: %d\n", cur_block, cur_block->prev, cur_block->next, cur_block->size);
		if(cur_block == cur_block->next) break;
		cur_block = cur_block->next;
	} 
	
} // END OF MEM_DUMP

int main(int argc, char **argv) {

	return 0;
} // END OF MAIN
