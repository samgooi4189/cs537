#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <math.h>
#include "mem.h"

#define NALLOC 4096

typedef struct{
	struct header *ptr;
	int size;
}header;

header base;	/* empty list to get started*/
header *freeptr = NULL;	/*start of free list*/

int m_error;

//init memory for the first time
int Mem_Init(int sizeOfRegion){
	//get page size
	int page_size = sysconf(_SC_PAGESIZE);
	//calculate the total region required
	sizeOfRegion = ceil(sizeOfRegion / page_size)  * page_size;

	int fd = open("/dev/zero", O_RDWR);

	/*if(freeptr == NULL){
	  freeptr = mmap(0, roundoff_Region, PROT_READ | PROT_WRITE, MAP_PRIVATE, 0, sysconf(_SC_PAGE_SIZE));
		if(freeptr == (void *) -1){
		  freeptr = NULL;
		  m_error = E_BAD_ARGS;
		  return -1;
		}
		
		freeptr->size  = roundoff_Region;
		freeptr->ptr = 		
		return 0;
	}

	return -1;*/

	void *ptr = mmap(NULL, sizeOfRegion, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (ptr == MAP_FAILED) { perror("mmap"); m_error = E_BAD_ARGS; return -1; }

	// close the device (don't worry, mapping should be unaffected)
	close(fd);
	return 0;
}

//allocate memory 
void *Mem_Alloc(int size){
	header *p, *prevptr;
	header *morecore(int);

	int nunits;

	nunits = size + sizeof(header);
	
	//create a list if not initialize yet
	if((prevptr = freeptr)==NULL){	/*not free list yet*/
		//base.ptr = freeptr = prevptr = &base;
		freeptr = prevptr = &base;
		base->ptr = base;
		base.size =0;
		if(Mem_Init(size) == -1){
			m_error = E_NO_SPACE;
			return NULL;	
		}
	}

	
	header *best_fit_ptr = freeptr ;
	int smallest_diff = freeptr->size;
	for (p = prevptr->ptr; ; prevptr = p, p = p->ptr){ 
		if(p->size >= nunits){	/*check is it big enough*/
			//if(p->s.size == nunits)	/*when size is exactly the same*/
			//	prevp->s.ptr = p->s.ptr;
			//else{			/*allocate tail end*/
			//	p->s.size -= nunits;
			//	p += p->s.size;
			//	p->s.size = nunits;
			//}
			//freep = prevp;
			//return (void *)(p+1);
			//check for the smallest best fit
			if(p->size < smallest_diff){
				best_fit_ptr = p;
				smallest_diff = diff_size;
			}
		}
	}
	//put the file in best_fit ptr
	
}

/*static header *morecore(int nu){
	char *cp, *sbrk(int);
	header *up;

	if(nu < NALLOC)
		nu = NALLOC;
	cp = sbrk(nu * sizeof(Header));

	if (cp == (char *) -1)	//if not space at all
		return NULL;
	
	up = (header *) cp;
	up->s.size = nu;
	free((void *)(up+1));
	return freep;
}*/

/*void free(void *ap){
	Header *bp, *p;

	bp = (Header *)ap - 1;	//point to the block header

	for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
		if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
			break;	//freed block at start or end of arena
	
	if (bp + bp->size == p->s.ptr){	//join to upper nbr
		bp->s.size += p->s.ptr->s.size;
		bp->s.ptr = p->s.ptr->s.ptr;
	}
	else
		bp->s.ptr = p->s.ptr;

	if(p + p->size == bp){		//join to lower nbr
		p->s.size += bp->s.size;
		p->s.ptr = bp->s.ptr;
	}
	else
		p->s.ptr = bp;
	
	freep = p;
}*/

int Mem_Free(void *ptr, int coalesce);

void Mem_Dump();


