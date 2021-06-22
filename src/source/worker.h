#ifndef SERVER_WORKER
#define SERVER_WORKER
#include "win32defs.h"

#include <stdlib.h>

#include "worker/worker_generics.h"
#include "file.h"
#include "net_msg.h"
#include "shared_state.h"
#include "log.h"

#define WORKER_IDLE 0
#define WORKER_DO 1
#define WORKER_DONE 2

typedef struct worker_data {
	//mux shared between main and given thread
	pthread_mutex_t thread_mux;
	pthread_cond_t thread_cond;
	int do_work; //condition variable condition check
	int exit;
	int in_conn;
	int add_conn;

	//mux not needed for this data
	file_t* files;
	size_t file_num;
	log_t* log;

	shared_state* shared;
} worker_data;

int init_worker_data(worker_data* wd, file_t* files, size_t file_num,
	log_t* log, shared_state* shared);
int destroy_worker_data(worker_data* wd);

/* thread function to be used in pthread_create call
 * args must be of type worker_data, returns number of requests served. */
void* worker_routine(void* args);

/* private functions implemented externally from the worker.c file. */
int do_acceptor(int* conn, int* newconn, log_t* log, shared_state* shared);
int do_close_connection(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state);
int do_open_file(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state);
int do_close_file(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state);
int do_read_file(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state);
int do_readn_files(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state);
int do_write_file(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state);
int do_append_file(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state);
int do_lock_file(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state);
int do_unlock_file(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state);
int do_remove_file(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state);

#endif
