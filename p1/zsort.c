#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define NUMRECS (24)

typedef struct __rec_t{
	unsigned int key;
	unsigned int record[NUMRECS];
} rec_t;


int
compare (const void * a, const void * b){
	rec_t a_rec = *(rec_t*)a;
	rec_t b_rec = *(rec_t*)b;
	return (a_rec.key - b_rec.key);
}

int get_file_size(const char *file) 
{
        struct stat buffer;
        if ( stat(file, &buffer) != 0 ) return(0);
        return( buffer.st_size );
}

void
usage(char *prog) 
{
    fprintf(stderr, "usage: %s <-i input file> <-o output file>\n", prog);
    exit(1);
}

int
main (int argc, char* argv[]){
	// program assumes a 4-byte key
	//printf("size: %d\n", sizeof(rec_t));
    	//assert(sizeof(rec_t) == 4096);
	
	// arguments
    	char *inFile = "";	
	char *outFile= "";

	//check for argc
	if(argc <2){
		usage(argv[0]);
		exit(1);
	}
	
    // input params
    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "i:o:")) != -1) {
		switch (c) {
			case 'i':
				inFile = strdup(optarg);
				break;
			case 'o':
				outFile = strdup(optarg);
				break;
			default:
				usage(argv[0]);
		}
    }
	
    // open file
    int fd = open(inFile, O_RDONLY);
    if (fd < 0) {
		fprintf(stderr, "Error: Cannot open file %s\n", inFile);
		exit(1);
    }

    //create output file if not exist
	int fout = open(outFile, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
	if(fout < 0){
		fprintf(stderr, "Error: Cannot open file %s\n", outFile);
		exit(1);
	}

	//get file size
	int filesize = get_file_size(inFile);

    	rec_t* r = malloc(filesize);
	int numrecord = filesize / sizeof(rec_t);
	int k = 0;
    while (1 && k < numrecord) {	
		int rc;
		rc = read(fd, &r[k], sizeof(rec_t));
		if (rc == 0) // 0 indicates EOF
			break;
		if (rc < 0) {
			perror("read");
			exit(1);
		}
		printf("key:%9d rec:", r[k].key);
		int j;
		for (j = 0; j < NUMRECS; j++) 
			printf("%3d ", r[k].record[j]);
		printf("\n");

		k++;
	}

	//now every records is in r, lets sort them.
	qsort(r, numrecord, sizeof(rec_t), compare);

	
	//write into fout
	k=0;
	printf("\nKeys sorted: \n");
	while(k < numrecord){
		printf("====%9d\n", r[k].key);
		int rc = write(fout, &r[k], sizeof(rec_t));
		if (rc != sizeof(rec_t)) {
			perror("write");
			exit(1);
			// should probably remove file here but ...
		}
		k++;
	}

	(void) close(fd);
	(void) close(fout);
	exit(0);
}
