#include <string.h>
#include "cs537.h"
#include "request.h"
#include <assert.h>

// 
// server.c: A very, very simple web server
//
// To run:
//  server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//
const char* fifo = "FIFO";
const char* sff = "SFF";
const char* sffbs = "SFF-BS";

// CS537: Parse the new arguments too
void getargs(int *port, int *workersCounts, int *bufferSize, char *scheduling, int *sffbsArg, int argc, char *argv[])
{
    if (argc < 5) {
	fprintf(stderr, "Usage: %s <port> <threads> <buffer> <scheduling> <N for sff-bs> \n", argv[0]);
	exit(1);
    }
    	 	 	   
    *port = atoi(argv[1]);
    *workersCounts = atoi(argv[2]);
    *bufferSize = atoi(argv[3]);
    scheduling = strdup(argv[4]);
    
    if (strcmp(scheduling, sff) != 0 && strcmp(scheduling, fifo) != 0 && strcmp(scheduling, sffbs) != 0) {
    	fprintf(stderr, "Invalid scheduling.");
    	exit(1);
    }
    
    // if scheduling is sff bs, there should be extra arg
    if(strcmp(scheduling, sffbs) == 0) {
    	
    	if(argv[5] == NULL) {
    		fprintf(stderr, "Usage: %s <port> <threads> <buffer> <scheduling> <N for sff-bs> \n", argv[0]);
			exit(1);
    	}
    	*sffbsArg = atoi(argv[5]);
    }
    
}

/* 
this is a function for worker thread
- get the file descriptor from the front of queue and handle the request
- use lock to achieve mutual exclusion and conditional variable to solve consumer/producer
*/
void* workerThread(void *arg) {

	worker_id *workerId = (worker_id *) arg;
	node_request *currRequest;
	
	while(1) {
		// get the lock
		pthread_mutex_lock(&(workerId->workerResources->bufferLock));
		
		while(workerId->workerResources->bufferFilled == 0) {
			pthread_cond_wait(&(workerId->workerResources->bufferNotEmpty), &(workerId->workerResources->bufferLock));
		}
		
		currRequest = workerId->workerResources->bufferFront;
		workerId->workerResources->bufferFilled--; // decrement the buffer
		if(workerId->workerResources->groupHead != NULL) {
			workerId->workerResources->groupHead = workerId->workerResources->groupHead->next;
		}
		
		if(currRequest->conn->is_static) {
			worker_thread *tempWorker = (worker_thread *) malloc(sizeof(worker_thread));
	    	tempWorker = &(workerId->workerResources->workerThreads[workerId->id]);    
			tempWorker->requestStatic++; // increment static counter
			tempWorker->requestHandled++;
			//free(tempWorker);
		} else {
			worker_thread *tempWorker = (worker_thread *) malloc(sizeof(worker_thread));
	    	tempWorker = &(workerId->workerResources->workerThreads[workerId->id]);    
			tempWorker->requestDynamic++; // increment dynamic counter	
			tempWorker->requestHandled++;	
			//free(tempWorker);
		}
		
		if(workerId->workerResources->bufferFilled == 0) {
			workerId->workerResources->bufferFront = NULL;
			workerId->workerResources->bufferEnd = NULL;
		} else {
			workerId->workerResources->bufferFront = currRequest->next;
		}
		
		if(workerId->workerResources->bufferFilled == workerId->workerResources->bufferSize-1) {
			pthread_cond_signal(&(workerId->workerResources->bufferEmpty));
		}
		
		// release the lock
		pthread_mutex_unlock(&(workerId->workerResources->bufferLock));
		
		requestHandle(currRequest->conn, workerId);
		Close(currRequest->conn->fd);
		//free(currRequest);
	}
}

void reorderSff(workers_resource *workerResources, node_request *groupHead, node_request *currRequest) {
	
	node_request *curr = workerResources->bufferFront;
		
	if(groupHead == workerResources->bufferFront) { // groupHeader is the bufferFront, only 1 group
		
		if(groupHead->conn->size >= currRequest->conn->size) {
			currRequest->next = groupHead;
			groupHead = currRequest; // update the groupHead
			workerResources->bufferFront = currRequest; // update the bufferFront
		} else {
			curr = groupHead; // curr now starts at groupHead
			while(curr != NULL) {
				if(curr->next == NULL || currRequest->conn->size < curr->next->conn->size) {
					currRequest->next = curr->next;
					curr->next = currRequest;
					break;
				}
				curr = curr->next;
			}
			
			if(currRequest->next == NULL) {
				workerResources->bufferEnd = currRequest;
			}
		}
	} else {
		
		// move this to a node before groupHead
		while(curr->next != groupHead && curr->next != NULL) {
			curr = curr->next;
		}
		
		if(groupHead->conn->size >= currRequest->conn->size) {
		
			currRequest->next = groupHead;
			curr->next = currRequest;
			groupHead = currRequest;
		
		} else {
			
			curr = groupHead; // curr now starts at groupHead
			while(curr != NULL) {
				if(curr->next == NULL || currRequest->conn->size < curr->next->conn->size) {
					currRequest->next = curr->next;
					curr->next = currRequest;
					break;
				}
				curr = curr->next;
			}
			
			if(currRequest->next == NULL) {
				workerResources->bufferEnd = currRequest;
			}
		}
	}
	
}

int main(int argc, char *argv[])
{
    int listenfd, port, workersCounts, bufferSize, sffbsArg, clientlen;
    char *scheduling = malloc(7);
    struct sockaddr_in clientaddr;
	workers_resource *workerResources;
	int jobTicket = 0;
    getargs(&port, &workersCounts, &bufferSize, scheduling, &sffbsArg, argc, argv);
    
    scheduling = strdup(argv[4]);
    workerResources = (workers_resource*) malloc(sizeof(workers_resource));
    workerResources->groupHead = (node_request *) malloc(sizeof(node_request)); // for sff-bs
    
    // check if malloc fails
    if(workerResources == NULL) {
    	fprintf(stderr, "Malloc failed.");
    	exit(1);
    }
    
    workerResources->workerThreads = (worker_thread *) malloc(sizeof(worker_thread) * workersCounts);
    
    if(workerResources->workerThreads == NULL) {
    	fprintf(stderr, "Malloc failed.");
    	exit(1);
    }
    
    
    workerResources->bufferFront = NULL;
    workerResources->bufferEnd = NULL;
    workerResources->bufferFilled = 0;
    workerResources->bufferSize = bufferSize;
    
    // init lock and cv
    if(pthread_mutex_init(&(workerResources->bufferLock), NULL) != 0) {
    	fprintf(stderr, "Fail to initialize lock.");
    }

    if(pthread_cond_init(&(workerResources->bufferEmpty), NULL) != 0) {
    	fprintf(stderr, "Fail to initialize conditional variable.");
    }
    
    if(pthread_cond_init(&(workerResources->bufferNotEmpty), NULL) != 0) {
    	fprintf(stderr, "Fail to initialize conditional variable.");
    }
    // 
    // CS537: Create some threads...
    //
    int i;
    for(i = 0; i < workersCounts; i++) {
 		
    	worker_thread *tempWorker = (worker_thread *) malloc(sizeof(worker_thread));
    	tempWorker = &(workerResources->workerThreads[i]);
    	tempWorker->requestHandled = 0;
    	tempWorker->requestStatic = 0;
    	tempWorker->requestDynamic = 0;    
    	tempWorker->thread = (pthread_t *) malloc(sizeof(pthread_t));	
    	worker_id *workerId = (worker_id *) malloc(sizeof(worker_id));
    	workerId->id = i; // set the id
    	workerId->workerResources = workerResources;
    	
    	if(pthread_create(tempWorker->thread, NULL, workerThread, workerId) != 0) {
    		fprintf(stderr, "Cannot create thread.");
    	}
    }
    
    listenfd = Open_listenfd(port);
    
    while (1) {
		clientlen = sizeof(clientaddr);
		
		node_request *currRequest = (node_request *) malloc(sizeof(node_request));
		currRequest->conn = (conn_request*) malloc(sizeof(conn_request));
		currRequest->conn->fd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
		requestInit(currRequest->conn); // set the conn with proper info
		
		//currRequest->next = NULL;
		
		// 
		// CS537: In general, don't handle the request in the main thread.
		// Save the relevant info in a buffer and have one of the worker threads 
		// do the work. However, for SFF, you may have to do a little work
		// here (e.g., a stat() on the filename) ...
		//
		
		pthread_mutex_lock(&(workerResources->bufferLock)); 
		
		while(workerResources->bufferFilled >= workerResources->bufferSize) {
			pthread_cond_wait(&(workerResources->bufferEmpty), &(workerResources->bufferLock));
		}
		
		// if nothing is in the buffer
		if(workerResources->bufferFilled == 0) {
		
			// fix for sffbs
			if(strcmp(scheduling, sffbs) == 0) {
				workerResources->groupHead = currRequest;
				jobTicket++;
			}
			
			workerResources->bufferFront = currRequest;
			workerResources->bufferEnd = currRequest;
			pthread_cond_signal(&(workerResources->bufferNotEmpty));
		} else {
		
			// in our sff, the shortest is in front of queue
			if(strcmp(scheduling, sff) == 0) {
				
				// put the request to the right position based on size
				node_request *temp = workerResources->bufferFront;
				
				if(temp->conn->size >= currRequest->conn->size) {
				
					currRequest->next = temp;
					workerResources->bufferFront = currRequest;
				
				} else {
								
					while(temp != NULL) {
						if(temp->next == NULL || currRequest->conn->size < temp->next->conn->size) {
							currRequest->next = temp->next;
							temp->next = currRequest;
							break;
						}
						temp = temp->next;
					}
				}
				
				
			} else if(strcmp(scheduling, fifo) == 0) { // if FIFO
				
				currRequest->next = NULL; // this will be last element in buffer queue
				workerResources->bufferEnd->next = currRequest; // add to the end
				workerResources->bufferEnd = currRequest; // update buffer end
				
			} else { //sff-bs
			
				if(jobTicket % sffbsArg == 0) { // get the head of new group
					workerResources->groupHead = currRequest;
					workerResources->bufferEnd->next = currRequest;
					workerResources->bufferEnd = currRequest;
				} else {
					reorderSff(workerResources, workerResources->groupHead, currRequest);
				}
				
				
				jobTicket++;
			}
		}
		
		workerResources->bufferFilled++; // increase the # of buffers filled
		pthread_mutex_unlock(&(workerResources->bufferLock));
		
    }

}


    


 
