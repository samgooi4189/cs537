#include <stdio.h>
#include "udp.h"
#include "mfs.h"
#include <sys/stat.h>

#define BUFFER_SIZE (4096)

MFS_Prot_t *prot_r;
int unwritten_bytes = 0;
int free_bytes = 0; 
int bytes_offset;
void *bytes_start_ptr;

void* allot_space(MFS_Header_t **header, int size, int *offset);
void prepare_inode(MFS_Inode_t* inode, int type, MFS_Inode_t* inode_old);
void update_inode(MFS_Header_t **header, int inode_num, int new_offset);
MFS_Inode_t *fix_inode(MFS_Header_t *header, int inode_num);
void verify(MFS_Header_t **header, void **block_ptr, int size);
int lookup(void *block_ptr, MFS_Inode_t *inode, char *name);
int gen_inum(MFS_Header_t **header, int offset);
void flush(int image_fd);
void write_header(int image_fd, MFS_Header_t *header);

int
main(int argc, char *argv[])
{
	// check for correct arguments
	if(argc != 3) {
		fprintf(stderr, "Usage: %s <port> <file-system-image>\n", argv[0]);
		exit(1);
	}
	
	int portnum = atoi(argv[1]);
	char *fs_image = argv[2];
    int sd = UDP_Open(portnum);
    assert(sd > -1);

	int fd = open(fs_image, O_RDWR|O_CREAT, S_IRWXU);
	if(fd < 0) {
		fprintf(stderr, "Cannot open file image");
		exit(1);
	}
	
	struct stat file_stat;
	if(fstat(fd, &file_stat) < 0) {
		fprintf(stderr, "Cannot open file image");
		exit(1);
	}
	
	int i, j, rc;
	MFS_Header_t *header;
	int image_size;
	free_bytes = MFS_BYTE_STEP_SIZE; 
	
	int entry_offset, inode_offset, new_dir_offset, parent_inode_offset;
	int tmp_offset, tmp_inode_offset, tmp_imap_offset;
	int done = 0;
	MFS_Imap_t *imap_temp;
	MFS_Inode_t *inode_temp;
	MFS_Inode_t *new_inode;
	MFS_DirEnt_t *entry_temp;
	
	if(file_stat.st_size >= sizeof(MFS_Header_t)) {
	
		image_size = file_stat.st_size + MFS_BYTE_STEP_SIZE;
		printf("Using old file of size %d\n", (int)file_stat.st_size);
		header = (MFS_Header_t *)malloc(image_size);
		// Put text in memory
		rc = read(fd, header, file_stat.st_size);
		
		if(rc < 0){
			fprintf(stderr, "Cannot open file");
			exit(1);
		}
	} else {
		//Initialize
		image_size = sizeof(MFS_Header_t) + MFS_BYTE_STEP_SIZE;
		header = (MFS_Header_t *)malloc(image_size);

		// root initialization
		inode_temp = allot_space(&header, sizeof(MFS_Inode_t), &tmp_inode_offset);
		imap_temp = allot_space(&header, sizeof(MFS_Imap_t), &tmp_imap_offset);
		imap_temp->inodes[0] = tmp_inode_offset;
		prepare_inode(inode_temp, MFS_DIRECTORY, NULL);

		for (i = 0; i < 14; i++) {
			imap_temp->inodes[i] = -1;
		}
		
		// header initialization
		for (i = 0; i < 4096/14; i++) {
			header->map[i] = -1;	
		}
	
		imap_temp->inodes[0] = tmp_inode_offset;

		// add two default entries
		entry_temp = allot_space(&header, MFS_BLOCK_SIZE, &tmp_offset);
		entry_temp[0].name[0] = '.';
		entry_temp[0].name[1] = '\0';
		entry_temp[0].inum = 0; 
		entry_temp[1].name[0] = '.';
		entry_temp[1].name[1] = '.';
		entry_temp[1].name[2] = '\0';
		entry_temp[1].inum = 0; 

		for (i = 2; i < MFS_BLOCK_SIZE/sizeof(MFS_DirEnt_t); i++) {
			entry_temp[i].inum = -1;
		}
		
		inode_temp->data[0] = tmp_offset;

		//Write to disk
		header->map[0] = tmp_imap_offset;
		flush(fd);		
		write_header(fd, header);
		printf("Initializing new file\n");
	}
	
	void* header_ptr = (void*)header;
	void* block_ptr = header_ptr + sizeof(MFS_Header_t);

	prot_r = (MFS_Prot_t*)malloc(sizeof(MFS_Prot_t));

	printf("Started listening at port %d\n", portnum);
	
    while (1) {
		struct sockaddr_in s;
		rc = UDP_Read(sd, &s, (char*)prot_r, sizeof(MFS_Prot_t));
		if (rc > 0) {
			
			//Special case for shutdown
			if(prot_r->cmd == CMD_INIT){
				printf("Server initialized\n");
				prot_r->ret = 0;
			} else if(prot_r->cmd == CMD_LOOKUP){
				
				prot_r->ret = -1;
				MFS_Inode_t* parent_inode = fix_inode(header, prot_r->pinum);
				prot_r->ret = lookup(block_ptr, parent_inode, &(prot_r->datapacket[0]));
			} else if(prot_r->cmd == CMD_SHUTDOWN){
				//Close file
				rc = close(fd);
				if(rc < 0){
					fprintf(stderr, "Cannot open file");
					exit(1);
				}
				prot_r->ret = 0;
				if(UDP_Write(sd, &s, (char*)prot_r, sizeof(MFS_Prot_t)) < -1){
					fprintf(stderr, "Unable to send result");
					exit(1);
				}
				exit(0);
			} else if(prot_r->cmd == CMD_UNLINK){
				
				verify(&header, &block_ptr, 16384);
				prot_r->ret = -1;
				MFS_Inode_t* parent_inode = fix_inode(header, prot_r->pinum);
				if(parent_inode != NULL && parent_inode->type == MFS_DIRECTORY){
					int exist = lookup(block_ptr, parent_inode, &(prot_r->datapacket[0]));
					if(exist != -1){
						//Check if empty
						MFS_Inode_t* this_inode = fix_inode(header, exist);
						if(!(this_inode->type == MFS_DIRECTORY && this_inode->size != 0)){
							//Need to remove
							MFS_DirEnt_t* new_dir_entry = allot_space(&header, MFS_BLOCK_SIZE, &entry_offset);
							MFS_Inode_t* new_parent_inode = allot_space(&header, sizeof(MFS_Inode_t), &parent_inode_offset);

							prepare_inode(new_parent_inode, 0, parent_inode);
							update_inode(&header, prot_r->pinum, parent_inode_offset);
							i = 0, done = 0;
							while(i < 14) {
								if(parent_inode->data[i] != -1){
									j = 0;
									while(j < MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t)){
										//printf("Parent node %d %d\n", inode->data[i], MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t) );
										MFS_DirEnt_t* entry = (MFS_DirEnt_t*)(block_ptr + parent_inode->data[i] + (j * sizeof(MFS_DirEnt_t)));			
										if(entry->inum != -1 && strcmp(entry->name, prot_r->datapacket) == 0 ){
											memcpy(new_dir_entry, block_ptr + parent_inode->data[i] , MFS_BLOCK_SIZE);
											//We now know which entry
											new_parent_inode->data[i] = entry_offset;
											new_dir_entry[j].inum = -1;
											update_inode(&header, exist, -1);
											prot_r->ret = 0;
											new_parent_inode->size--;
											done = 1;
											break;
										}
										j++;
									}
									if(done == 1) break;
								}
								i++;
							}


						}

					}else{
						prot_r->ret = 0;
					}
				}

			} else if(prot_r->cmd == CMD_READ){
				
				prot_r->ret = -1;
				MFS_Inode_t* parent_inode = fix_inode(header, prot_r->pinum);
				if(parent_inode != NULL && parent_inode->type == MFS_REGULAR_FILE && prot_r->block >= 0 && prot_r->block < 14){
					//New inode
					memcpy(prot_r->datapacket, block_ptr + parent_inode->data[prot_r->block], MFS_BLOCK_SIZE);
					prot_r->ret = 0;
				}
			} else if(prot_r->cmd == CMD_STAT){
				
				prot_r->ret = -1;
				MFS_Inode_t* parent_inode = fix_inode(header, prot_r->pinum);
				if(parent_inode != NULL && prot_r->block >= 0 && prot_r->block < 14){
					//New inode
					prot_r->block = parent_inode->size;
					prot_r->datapacket[0] = parent_inode->type;
					prot_r->ret = 0;
				}
			} else if(prot_r->cmd == CMD_WRITE){
				
				verify(&header, &block_ptr, 16384);
				prot_r->ret = -1;
				MFS_Inode_t* parent_inode = fix_inode(header, prot_r->pinum);
				int block_offset;
				if(parent_inode != NULL && parent_inode->type == MFS_REGULAR_FILE && prot_r->block >= 0 && prot_r->block < 14){
					//New inode
					new_inode = (MFS_Inode_t*)allot_space(&header, sizeof(MFS_Inode_t), &inode_offset);
					prepare_inode(new_inode, 0, parent_inode);
					void* new_block = allot_space(&header, MFS_BLOCK_SIZE, &block_offset);

					memcpy(new_block, prot_r->datapacket, MFS_BLOCK_SIZE);
					i = prot_r->block;
					while(new_inode->data[i] == -1 && i >= 0){
						new_inode->size += MFS_BLOCK_SIZE;
						new_inode->data[i] = block_offset; 
						i--;
					}
					new_inode->data[prot_r->block] = block_offset; 
					update_inode(&header, prot_r->pinum, inode_offset);
					prot_r->ret = 0;
				}

			} else if(prot_r->cmd == CMD_CREAT){
				
				verify(&header, &block_ptr, 16384);
				prot_r->ret = -1;

				MFS_Inode_t* parent_inode = fix_inode(header, prot_r->pinum);
				int exist = lookup(block_ptr, parent_inode, &(prot_r->datapacket[1]));

				if(exist == -1){

					new_inode = allot_space(&header, sizeof(MFS_Inode_t), &inode_offset);
					prepare_inode(new_inode, prot_r->datapacket[0], NULL);
					int new_inode_inum = gen_inum(&header, inode_offset);


					if(parent_inode != NULL && parent_inode->type == MFS_DIRECTORY && strlen(&(prot_r->datapacket[1])) <= 28 && new_inode_inum != -1){
						//Check if the dir is full
						MFS_DirEnt_t* entry;
						//Initialize new data block for entries
						MFS_DirEnt_t* new_entry =  allot_space(&header, MFS_BLOCK_SIZE, &entry_offset);

						MFS_Inode_t* new_parent_inode = allot_space(&header, sizeof(MFS_Inode_t), &parent_inode_offset);
						prepare_inode(new_parent_inode, 0, parent_inode);
						update_inode(&header, prot_r->pinum, parent_inode_offset);

						//Copy new stuff	
						done = 0;
						i = 0;
						while(i < 14) {
							if(parent_inode->data[i] != -1){

								j = 0;
								while(j < MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t)){
									entry = (MFS_DirEnt_t*)(block_ptr + parent_inode->data[i] + (j * sizeof(MFS_DirEnt_t)));			
									if(entry->inum == -1){

										//Copy the dir entry
										memcpy(new_entry, block_ptr + parent_inode->data[i], MFS_BLOCK_SIZE);
										new_parent_inode->data[i] = entry_offset;
										new_entry[j].inum = new_inode_inum;	
										strcpy(new_entry[j].name, &(prot_r->datapacket[1]));
										//printf("Name: %s - %s\n",entry->name, &(prot_r->datapacket[1]));
										done = 1;
										break;
									}
									j++;
								}
								if(done == 1) break;
							}else{	

								//Create new node
								//Initialize
								for (j = 0; j < MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t); j++) {
									new_entry[j].inum = -1;
								}
								new_parent_inode->data[i] = entry_offset;
								new_entry[0].inum = new_inode_inum;			
								strcpy(new_entry[0].name, &(prot_r->datapacket[1]));
								done = 1;
								break;
							}
							i++;
						}
						if(done){
							//Actually create the inode
							//Add .. and . dirs
							if(new_inode->type == MFS_DIRECTORY){
								MFS_DirEnt_t* new_dir_entry =  allot_space(&header, MFS_BLOCK_SIZE, &new_dir_offset);
								for (i = 0; i < MFS_BLOCK_SIZE/sizeof(MFS_DirEnt_t); i++) {
									new_dir_entry[i].inum = -1;
								}
								new_dir_entry[0].name[0] = '.';
								new_dir_entry[0].name[1] = '\0';
								new_dir_entry[0].inum = new_inode_inum; 
								new_dir_entry[1].name[0] = '.';
								new_dir_entry[1].name[1] = '.';
								new_dir_entry[1].name[2] = '\0';
								new_dir_entry[1].inum = prot_r->pinum; 
								new_inode->data[0] = new_dir_offset;
							}	

							//Write to block
							new_parent_inode->size++;
							header->total_inode++;
							prot_r->ret = 0;
						}else{
							header->total_byte -= unwritten_bytes;
							unwritten_bytes = 0;
						}
					}else{
						header->total_byte -= unwritten_bytes;
						unwritten_bytes = 0;
					}
				}else{
					prot_r->ret = 0;
				}

			} else {
				fprintf(stderr, "Unknown command");
				exit(1);
				continue;
			}

			flush(fd);
			write_header(fd, header);
			if(UDP_Write(sd, &s, (char*)prot_r, sizeof(MFS_Prot_t)) < -1){
				fprintf(stderr, "Unable to send result");
				exit(1);
			}
		}

    }

    return 0;
}

void* allot_space(MFS_Header_t **header, int size, int *offset) {
	void *ptr = *header;
	if(unwritten_bytes == 0) {
		bytes_start_ptr = ptr + sizeof(MFS_Header_t) + (*header)->total_byte;
		bytes_offset = (*header)->total_byte;
	}
	
	if(free_bytes <= size) {
		
		*header = realloc(*header, sizeof(MFS_Header_t) + (*header)->total_byte + size + free_bytes + MFS_BYTE_STEP_SIZE);
		free_bytes = free_bytes + MFS_BYTE_STEP_SIZE;
	} else {
		free_bytes -= size;
	}
	
	if(offset != NULL) {
		*offset = (*header)->total_byte;
	}
	
	ptr = (void *)ptr + sizeof(MFS_Header_t) + (*header)->total_byte;
	unwritten_bytes += size;
	(*header)->total_byte += size;
	return ptr;
}

// help to initialize inode based on type
void prepare_inode(MFS_Inode_t* inode, int type, MFS_Inode_t* inode_old) {

	if(inode_old != NULL) {
		memcpy(inode, inode_old, sizeof(MFS_Inode_t));
	} else {
		inode->size = 0;
		inode->type = type;
		int i;
		for(i = 0; i < 14; i++) {
			inode->data[i] = -1;
		}
	}
}

void update_inode(MFS_Header_t **header, int inode_num, int new_offset) {
	MFS_Header_t *headerp = *header;
	void *header_ptr = (void *)headerp;
	MFS_Imap_t *old_imap = (MFS_Imap_t *)(header_ptr + sizeof(MFS_Header_t) + headerp->map[inode_num / 14]);
	int offset;
	MFS_Imap_t *imap = allot_space(header, sizeof(MFS_Imap_t), &offset);
	memcpy(imap, old_imap, sizeof(MFS_Imap_t));
	headerp->map[inode_num / 14] = offset;
	imap->inodes[inode_num % 14] = new_offset;
}

MFS_Inode_t *fix_inode(MFS_Header_t *header, int inode_num) {
	if(header -> map[inode_num / 14] != -1) {
		void *header_ptr = (void *) header;
		MFS_Imap_t *imap = (MFS_Imap_t *)(header_ptr + sizeof(MFS_Header_t) + header->map[inode_num / 14]);
		
		if (imap->inodes[inode_num % 14] == -1) {
			return NULL;
		} else {
			return (MFS_Inode_t *)(header_ptr + sizeof(MFS_Header_t) + imap->inodes[inode_num % 14]);
		}
	}
	return NULL;
}

void verify(MFS_Header_t **header, void **block_ptr, int size) {
	if(free_bytes <= size) {
		void *old_header = *header;
		*header = realloc(*header, sizeof(MFS_Header_t) + (*header)->total_byte + size + free_bytes + MFS_BYTE_STEP_SIZE);
		free_bytes = free_bytes + MFS_BYTE_STEP_SIZE;
		*block_ptr = (void*)(*header) + sizeof(MFS_Header_t);
		if(old_header != *header) {
		
		}
	}
}

int lookup(void *block_ptr, MFS_Inode_t *inode, char *name) {
	int i = 0, j;
	
	if(inode != NULL && inode->type == MFS_DIRECTORY) {
		while(i < 14) {
			if(inode->data[i] != -1) {
				j = 0;
				while(j < MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t)) {
					MFS_DirEnt_t *entry = (MFS_DirEnt_t *)(block_ptr + inode->data[i] + (j * sizeof(MFS_DirEnt_t)));
										
					if(entry->inum != -1 && strcmp(entry->name, name) == 0) {
						return entry->inum;
					}
					j++;
				}
			}
			i++;
		}
	}
	return -1;
}

int gen_inum(MFS_Header_t **header, int offset) {
	int i, j, tmp_offset;
	MFS_Imap_t *imap_temp;
	void *block_ptr = (void *)(*header) + sizeof(MFS_Header_t);
	i = 0;
	while(i < 4096/ 14) {

		if((*header)->map[i] == -1) {
			imap_temp = allot_space(header, sizeof(MFS_Imap_t), &tmp_offset);
			
			for(j = 0; j < 14; j++) {
				imap_temp->inodes[j] = -1;
			}
			
			imap_temp->inodes[0] = offset;
			(*header)->map[i] = tmp_offset;
			
			return (i * 14) + 0;
		} else {
			imap_temp = (MFS_Imap_t *)(block_ptr + (*header)->map[i]);
			
			for(j = 0; j < 14; j++) {
				if(imap_temp->inodes[j] == -1) {
					update_inode(header, (i * 14) + j, offset);
					return (i * 14) + j;
				}
			}
		}
		i++;
	}
	return -1;
}

void flush(int image_fd) {
	int rc = pwrite(image_fd, bytes_start_ptr, unwritten_bytes, sizeof(MFS_Header_t) + bytes_offset);
	if(rc < 0) {
		fprintf(stderr, "Error: mfs_flush - writing block");
		exit(1);
	}
	unwritten_bytes = 0;
}

void write_header(int image_fd, MFS_Header_t *header) {
	int rc = pwrite(image_fd, header, sizeof(MFS_Header_t), 0);
	if(rc < 0) {
		fprintf(stderr, "Error: mfs_write_header - writing header");
		exit(1);
	}
	fsync(image_fd); // force write   
}

