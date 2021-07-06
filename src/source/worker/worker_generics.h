#ifndef SERVER_WORKER_GENERICS
#define SERVER_WORKER_GENERICS
#include "../win32defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "../file.h"
#include "../net_msg.h"
#include "../log.h"
#include "../shared_state.h"

#define ALGO_FIFO 0
#define ALGO_LRU 1
#define ALGO_LFU 2

#define MESSAGE_MAX_SIZE FILE_NAME_MAX_SIZE

/* all the functions return 0 on success, -1 on failure setting errno. */

/* returns the index of the first non existing file, or -1 in case of error.
 * if error and errno == EMFILE, all slots are full. */
int get_empty_slot(file_t* files, size_t file_num);

/* returns the index of the matching file or -1 in case of error.
 * if error and errno == ENOENT, there was not file. */
int get_file(file_t* files, size_t file_num, const char* filename);

int read_file_name(net_msg* msg, char* buffer_name, size_t buffer_size);

/* cache miss functions. */
int cache_miss(log_t* log, int thread, char* nodel_file, file_t* files, size_t file_num, shared_state* state, net_msg* out_msg);
int evict_FIFO(log_t* log, int thread, char* nodel_file, file_t* files, size_t file_num, shared_state* state,
	void** buf, size_t* buf_size, char** name, size_t* name_size);
int evict_LRU(log_t* log, int thread, char* nodel_file, file_t* files, size_t file_num, shared_state* state,
	void** buf, size_t* buf_size, char** name, size_t* name_size);
int evict_LFU(log_t* log, int thread, char* nodel_file, file_t* files, size_t file_num, shared_state* state,
	void** buf, size_t* buf_size, char** name, size_t* name_size);
int delete_evicted(log_t* log, int thread, int file, file_t* files, shared_state* state,
	void** buf, size_t* buf_size, char** name, size_t* name_size);
int convert_slashes_to_underscores(char* name);

void do_log(log_t* log, int thread, int client, char* file, char* operation,
	char* outcome, int readsize, int writesize);
void do_log_main(log_t* log, int max_size, int max_files,
	int max_conns, int cache_miss);

#define SET_EMPTY_STRING(str) str[0] = '\0';

#endif