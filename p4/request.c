//
// request.c: Does the bulk of the work for the web server.
// 
#include <assert.h>
#include <sys/types.h>
#include "cs537.h"
#include "request.h"

void requestError(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{
   char buf[MAXLINE], body[MAXBUF];

   printf("Request ERROR\n");

   // Create the body of the error message
   sprintf(body, "<html><title>CS537 Error</title>");
   sprintf(body, "%s<body bgcolor=""fffff"">\r\n", body);
   sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
   sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
   sprintf(body, "%s<hr>CS537 Web Server\r\n", body);

   // Write out the header information for this response
   sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
   Rio_writen(fd, buf, strlen(buf));
   printf("%s", buf);

   sprintf(buf, "Content-Type: text/html\r\n");
   Rio_writen(fd, buf, strlen(buf));
   printf("%s", buf);

   sprintf(buf, "Content-Length: %lu\r\n\r\n", strlen(body));
   Rio_writen(fd, buf, strlen(buf));
   printf("%s", buf);

   // Write out the content
   Rio_writen(fd, body, strlen(body));
   printf("%s", body);

}


//
// Reads and discards everything up to an empty text line
//
void requestReadhdrs(rio_t *rp)
{
   char buf[MAXLINE];

   Rio_readlineb(rp, buf, MAXLINE);
   while (strcmp(buf, "\r\n")) {
      Rio_readlineb(rp, buf, MAXLINE);
   }
   return;
}

//
// Return 1 if static, 0 if dynamic content
// Calculates filename (and cgiargs, for dynamic) from uri
//
int requestParseURI(char *uri, char *filename, char *cgiargs) 
{
   char *ptr;

   if (!strstr(uri, "cgi")) {
      // static
      strcpy(cgiargs, "");
      sprintf(filename, ".%s", uri);
      if (uri[strlen(uri)-1] == '/') {
         strcat(filename, "home.html");
      }
      return 1;
   } else {
      // dynamic
      ptr = index(uri, '?');
      if (ptr) {
         strcpy(cgiargs, ptr+1);
         *ptr = '\0';
      } else {
         strcpy(cgiargs, "");
      }
      sprintf(filename, ".%s", uri);
      return 0;
   }
}

//
// Fills in the filetype given the filename
//
void requestGetFiletype(char *filename, char *filetype)
{
   if (strstr(filename, ".html")) 
      strcpy(filetype, "text/html");
   else if (strstr(filename, ".gif")) 
      strcpy(filetype, "image/gif");
   else if (strstr(filename, ".jpg")) 
      strcpy(filetype, "image/jpeg");
   else 
      strcpy(filetype, "test/plain");
}

double convertToSecond(struct timeval t) {
    int rc = gettimeofday(&t, NULL);
    assert(rc == 0);
    return (double) ((double)t.tv_sec + (double)t.tv_usec / 1e6);
}

void requestServeDynamic(int fd, char *filename, char *cgiargs, worker_id *workerId)
{
   char buf[MAXLINE], *emptylist[] = {NULL};

   // The server does only a little bit of the header.  
   // The CGI script has to finish writing out the header.
   sprintf(buf, "HTTP/1.0 200 OK\r\n");
   sprintf(buf, "%sServer: CS537 Web Server\r\n", buf);

	 /* CS537: Your statistics go here -- fill in the 0's with something useful! */
	 struct timeval reqArrival, reqDispatch;
	 int rc;
	 rc = gettimeofday(&reqArrival, NULL);
	 assert(rc == 0);
	 rc = gettimeofday(&reqDispatch, NULL);
	 assert(rc == 0);
	 
	worker_thread *tempWorker = (worker_thread *) malloc(sizeof(worker_thread));
	tempWorker = &(workerId->workerResources->workerThreads[workerId->id]);    	
	
	 sprintf(buf, "%s Stat-req-arrival: %ld\r\n", buf, (long)0);
	 sprintf(buf, "%s Stat-req-dispatch: %d\r\n", buf, 0);
	 sprintf(buf, "%s Stat-thread-id: %d\r\n", buf, workerId->id);
	 sprintf(buf, "%s Stat-thread-count: %d\r\n", buf, tempWorker->requestHandled);
	 sprintf(buf, "%s Stat-thread-static: %d\r\n", buf, tempWorker->requestStatic);
	 sprintf(buf, "%s Stat-thread-dynamic: %d\r\n", buf, tempWorker->requestDynamic);

   Rio_writen(fd, buf, strlen(buf));

   if (Fork() == 0) {
      /* Child process */
      Setenv("QUERY_STRING", cgiargs, 1);
      /* When the CGI process writes to stdout, it will instead go to the socket */
      Dup2(fd, STDOUT_FILENO);
      Execve(filename, emptylist, environ);
   }
   Wait(NULL);
}


void requestServeStatic(int fd, char *filename, int filesize, worker_id *workerId) 
{
   int srcfd;
   char *srcp, filetype[MAXLINE], buf[MAXBUF];

   requestGetFiletype(filename, filetype);

   srcfd = Open(filename, O_RDONLY, 0);

   // Rather than call read() to read the file into memory, 
   // which would require that we allocate a buffer, we memory-map the file
   srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
   Close(srcfd);

   // put together response
   sprintf(buf, "HTTP/1.0 200 OK\r\n");
   sprintf(buf, "%sServer: CS537 Web Server\r\n", buf);

	 // CS537: Your statistics go here -- fill in the 0's with something useful!
	 struct timeval reqArrival, reqDispatch;
	 int rc;
	 rc = gettimeofday(&reqArrival, NULL);
	 assert(rc == 0);
	 rc = gettimeofday(&reqDispatch, NULL);
	 assert(rc == 0);

	worker_thread *tempWorker = (worker_thread *) malloc(sizeof(worker_thread));
	tempWorker = &(workerId->workerResources->workerThreads[workerId->id]);    	
	 sprintf(buf, "%s Stat-req-arrival: %ld\r\n", buf, (long) convertToSecond(reqArrival));
	 sprintf(buf, "%s Stat-req-dispatch: %d\r\n", buf, convertToSecond(reqDispatch));
	 sprintf(buf, "%s Stat-req-read: %d\r\n", buf, 0);
	 sprintf(buf, "%s Stat-req-complete: %d\r\n", buf, 0); 
	 sprintf(buf, "%s Stat-thread-id: %d\r\n", buf, workerId->id);
	 sprintf(buf, "%s Stat-thread-count: %d\r\n", buf, tempWorker->requestHandled);
	 sprintf(buf, "%s Stat-thread-static: %d\r\n", buf, tempWorker->requestStatic);
	 sprintf(buf, "%s Stat-thread-dynamic: %d\r\n", buf, tempWorker->requestDynamic);

   sprintf(buf, "%sContent-Length: %d\r\n", buf, filesize);
   sprintf(buf, "%sContent-Type: %s\r\n\r\n", buf, filetype);

   Rio_writen(fd, buf, strlen(buf));

   //  Writes out to the client socket the memory-mapped file 
   Rio_writen(fd, srcp, filesize);
   Munmap(srcp, filesize);

}

// init a request
void requestInit(conn_request* conn)
{

   char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
   rio_t rio;

   Rio_readinitb(&rio, conn->fd);
   Rio_readlineb(&rio, buf, MAXLINE);
   sscanf(buf, "%s %s %s", method, uri, version);

   printf("%s %s %s\n", method, uri, version);

   if (strcasecmp(method, "GET")) {
      requestError(conn->fd, method, "501", "Not Implemented", "CS537 Server does not implement this method");
      return;
   }
   requestReadhdrs(&rio);

   conn->is_static = requestParseURI(uri, conn->filename, conn->cgiargs);
   if (stat(conn->filename, &conn->sbuf) < 0) {
      requestError(conn->fd, conn->filename, "404", "Not found", "CS537 Server could not find this file");
      return;
   }
	
   conn->size = conn->sbuf.st_size;
   if (conn->is_static) {
      if (!(S_ISREG(conn->sbuf.st_mode)) || !(S_IRUSR & conn->sbuf.st_mode)) {
         requestError(conn->fd, conn->filename, "403", "Forbidden", "CS537 Server could not read this file");
         return;
      }
   } else {
      if (!(S_ISREG(conn->sbuf.st_mode)) || !(S_IXUSR & conn->sbuf.st_mode)) {
         requestError(conn->fd, conn->filename, "403", "Forbidden", "CS537 Server could not run this CGI program");
         return;
      }
   }
}

// handle a request
void requestHandle(conn_request* conn, worker_id *workerId)
{
   if (conn->is_static) {
      requestServeStatic(conn->fd, conn->filename, conn->size, workerId);
   } else {
      requestServeDynamic(conn->fd, conn->filename, conn->cgiargs, workerId);
   }
}


