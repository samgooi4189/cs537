#ifndef __REQUEST_H__

typedef struct __conn_request {
	int fd;
	int is_static;
	struct stat sbuf;
	int size;
	char cgiargs[8192];
	char filename[8192];
} conn_request;

typedef struct __node_request {
	conn_request* conn;
	struct __node_request *next;
} node_request;

typedef struct __worker_thread {
	pthread_t *thread;
	int requestHandled;
	int requestStatic;
	int requestDynamic;
} worker_thread;

typedef struct __workers_resource {
	node_request *bufferFront;
	node_request *bufferEnd;
	node_request *groupHead; // useful for sff-bs
	int bufferFilled;
	int bufferSize; // max size of buffer
	worker_thread *workerThreads;
	pthread_mutex_t bufferLock;
	pthread_cond_t bufferEmpty;
	pthread_cond_t bufferNotEmpty;
} workers_resource;

typedef struct __worker_id {
	int id;
	workers_resource *workerResources;
} worker_id;

void requestInit(conn_request* conn);
void requestHandle(conn_request* conn, worker_id *workerId);

#endif
