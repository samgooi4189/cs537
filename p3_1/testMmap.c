#include <stdio.h>
#include <unistd.h>

typedef long Align;
union header {
	struct {
		union header *ptr;
		unsigned size;
	}s;
	Align x;
};
typedef union header Header;

int main (void){
	printf("The page size for this system. is %ld bytes.\n", 
		sysconf(_SC_PAGESIZE));

	return 0;
}
