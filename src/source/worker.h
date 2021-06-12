#ifndef SERVER_WORKER
#define SERVER_WORKER

#include <stdlib.h>
#include "file.h"

typedef struct worker_data {
	file_t* files;
	size_t file_num;
} worker_data;

/* thread function to be used in pthread_create call
 * args must be of type worker_data */
void* worker_routine(void* args);

/* this is an internal function used by the worker to handle a request.
 * lastop_writefile_pname contains the pathname of the file if the last operation
 * was an open with O_CREATE and O_LOCK flags and had success. */
static int worker_do(int conn, file_t* files, size_t file_num,
	char* lastop_writefile_pname);
