#ifndef SERVER_WORKER
#define SERVER_WORKER
#include "win32defs.h"

#include <stdlib.h>

#include "file.h"
#include "log.h"

typedef struct worker_data {
	pthread_mutex_t thread_mux;
	pthread_cond_t thread_cond;
	int do_work; //condition variable condition check
	int exit;
	int work_conn;
	char* work_conn_lastop;

	file_t* files;
	size_t file_num;
	log_t* log;

	long max_storage;
	int max_files;
	char cache_miss_algorithm;
	long current_storage;
	int current_files;
	pthread_mutex_t* state_mux;
} worker_data;

/* thread function to be used in pthread_create call
 * args must be of type worker_data */
void* worker_routine(void* args);

#endif
