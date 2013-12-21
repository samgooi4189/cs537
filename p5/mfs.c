#include <stdio.h> 
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include "mfs.h"
#include "udp.h"

int sd = -1;
int rc = 0;
struct sockaddr_in saddr;
char* mfs_hostname;
int mfs_portnum = 0;
MFS_Prot_t* prot_t;
MFS_Prot_t* prot_r;

/*
helper method for udp_read
*/
int UDP_helper(void *t, void *r, int attempt, int timeout){
	int sd = UDP_Open(0);
    if(sd < -1) {
        perror("Cannot open connection.\n");
        return -1;
    }

    struct sockaddr_in addr, addr2;
    int rc = UDP_FillSockAddr(&addr, mfs_hostname, mfs_portnum);
    if(rc < 0) {
        perror("Cannot find host.\n");
        return -1;
    }

    
    struct timeval tv;
    tv.tv_sec=timeout;
    tv.tv_usec=0;
	fd_set rfds; // file descriptor set

    while(1) {
        FD_ZERO(&rfds); // initialize the fd set to no file descriptors
        FD_SET(sd,&rfds); // add file descriptor to set
        UDP_Write(sd, &addr, (char*)t, sizeof(MFS_Prot_t));
        if(select(sd+1, &rfds, NULL, NULL, &tv))
        {
            rc = UDP_Read(sd, &addr2, (char*)r, sizeof(MFS_Prot_t));
            if(rc > 0){
                UDP_Close(sd);
                return 0;
            }
        }else {
            attempt -= 1;
        }
    };

}

/*
takes a host name and port number and uses those to find the server 
exporting the file system
*/
int MFS_Init(char *hostname, int portnum){
	if(portnum != 0){
		free(prot_t);
		free(prot_r);
		free(mfs_hostname);
	}
	
	int len = strlen(hostname) + 1;
	mfs_hostname = (char*)malloc(len);
	strcpy(mfs_hostname, hostname);
	mfs_portnum = portnum;
	prot_t = (MFS_Prot_t*)malloc(sizeof(MFS_Prot_t));
	prot_r = (MFS_Prot_t*)malloc(sizeof(MFS_Prot_t));
	prot_t->cmd = CMD_INIT;
	

	rc = UDP_helper(prot_t, prot_r, 10, 5);
	// check return code
	if (rc >= 0) {
		// get something back
		return prot_r->ret;
	}

	return -1;
}

/*
takes the parent inode number (which should be the inode number of a directory) and 
looks up the entry name in it. The inode number of name is returned. 
Success: return inode number of name; failure: return -1. 
*/
int MFS_Lookup(int pinum, char *name){

	prot_t->pinum = pinum;
	strcpy(prot_t->datapacket, name);
	prot_t->cmd = CMD_LOOKUP;

	rc = UDP_helper(prot_t, prot_r, 10, 5);
	//validate
	if(rc >= 0){
		return prot_r->ret;
	}
	return -1;
}

/*
returns some information about the file specified by inum. Upon success, return 0, 
otherwise -1. The exact info returned is defined by MFS_Stat_t. 
Failure modes: inum does not exist.
*/
int MFS_Stat(int inum, MFS_Stat_t *m){

	prot_t->pinum = inum;
	prot_t->cmd = CMD_STAT;
	
	rc = UDP_helper(prot_t, prot_r, 10, 5);
	// validate
	if(rc >= 0){
		m->type = prot_r->datapacket[0];
		m->size = prot_r->block;
		return prot_r->ret;
	}
	return -1;
}

/*
writes a block of size 4096 bytes at the block offset specified by block . 
Returns 0 on success, -1 on failure. Failure modes: invalid inum, invalid block, 
not a regular file (because you can't write to directories).
*/
int MFS_Write(int inum, char *buffer, int block){

	prot_t->pinum = inum;
	prot_t->block = block;
	prot_t->cmd = CMD_WRITE;
	memcpy(prot_t->datapacket, buffer, MFS_BLOCK_SIZE);

	rc = UDP_helper(prot_t, prot_r, 10, 5);
	// validate
	if(rc >= 0){
		return prot_r->ret;
	}
	return -1;
}

/*
reads a block specified by block into the buffer from file specified by inum . 
The routine should work for either a file or directory; directories should return 
data in the format specified by MFS_DirEnt_t. Success: 0, failure: -1. 
Failure modes: invalid inum, invalid block.
*/
int MFS_Read(int inum, char *buffer, int block){

	prot_t->pinum = inum;
	prot_t->block = block;
	prot_t->cmd = CMD_READ;

	rc = UDP_helper(prot_t, prot_r, 10, 5);
	if(rc >= 0){
		memcpy(buffer, prot_r->datapacket, MFS_BLOCK_SIZE);
		return prot_r->ret;
	}
	return -1;
}

/*
makes a file ( type == MFS_REGULAR_FILE) or directory ( type == MFS_DIRECTORY) in the 
parent directory specified by pinum of name name . Returns 0 on success, -1 on failure. 
Failure modes: pinum does not exist, or name is too long. If name already exists, 
return success (think about why).
*/
int MFS_Creat(int pinum, int type, char *name){

	prot_t->pinum = pinum;
	strcpy(prot_t->datapacket + sizeof(char), name);
	prot_t->datapacket[0] = (char)type;
	prot_t->cmd = CMD_CREAT;

	rc = UDP_helper(prot_t, prot_r, 10, 5);
	if(rc >= 0){
		return prot_r->ret;
	}
	return -1;
}

/*
removes the file or directory name from the directory specified by pinum . 
0 on success, -1 on failure. Failure modes: pinum does not exist, directory is NOT empty. 
Note that the name not existing is NOT a failure by our definition 
(think about why this might be).
*/
int MFS_Unlink(int pinum, char *name){
	
	prot_t->pinum = pinum;
	prot_t->cmd = CMD_UNLINK;
	strcpy(prot_t->datapacket, name);

	rc = UDP_helper(prot_t, prot_r, 10, 5);
	if(rc >= 0){
		return prot_r->ret;
	}
	return -1;
}

/*
 just tells the server to force all of its data structures to disk and shutdown by 
 calling exit(0). This interface will mostly be used for testing purposes.
 */
int MFS_Shutdown(){
	
	prot_t->cmd = CMD_SHUTDOWN;
	rc = UDP_helper(prot_t, prot_r, 10, 5);
	if(rc >= 0){
		return prot_r->ret;
	}
	return -1;
}

