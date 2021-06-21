#ifndef FILESYSTEM_FILE
#define FILESYSTEM_FILE
#include "win32defs.h"

#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>

#define OWNER_NEXIST -1
#define OWNER_NULL 0
#define FILE_NAME_MAX_SIZE 256

typedef struct file_struct {
	int owner;
	pthread_mutex_t mutex;
	char name[FILE_NAME_MAX_SIZE];
	size_t data_size;
	void* data;
	ssize_t open_size;
	void* open_data;
	size_t new_size; //bytes added since last open.

	//cache miss algorithms data
	time_t fifo_creation_time;
	int lfu_frequency;
	char lru_clock;	//second chance
} file_t;

/* all functions return 0 on success, -1 on failure setting errno. */

/* Sets as a non existing file. */
int init_file_struct(file_t* file);
int destroy_file_struct(file_t* file);
int reset_file_struct(file_t* file);

/* initializes the file struct with an empty file. */
int create_file_struct(file_t* file, char const* filename, int owner);

int is_existing_file(file_t* file);

/* returns 0 on name match, 1 on mismatch, -1 on error and sets errno. */
int check_file_name(file_t* file, char const* name);

/* returns 0 on file open, 1 on file closed, -1 on error and sets errno. */
int is_open_file(file_t* file);

/* returns 0 on file locked, 1 on file unlocked, -1 on error and sets errno. */
int is_locked_file(file_t* file, int who);

int get_size(file_t* file, size_t* data_size);

/* gets the usage data from the given file. at least one
 * between fifo, lfu, lru shall be not null. */
int get_usage_data(file_t* file, time_t* fifo, int* lfu, char* lru);

int update_lru(file_t* file, char newval);

int open_file(file_t* file, int who);
int close_file(file_t* file, int who, long max_size, long* curr_size, pthread_mutex_t* state_mux);

/* read the whole file into the given buffer, reallocating if necessary. */
int read_file(file_t* file, int who, void** out_data, size_t* out_data_size);

int write_file(file_t* file, int who, const void* data, size_t data_size);
int append_file(file_t* file, int who, const void* data, size_t data_size);

int lock_file(file_t* file, int who);
int unlock_file(file_t* file, int who);
int remove_file(file_t* file, int who);

/* forcefully removes a file, regardless of its state. */
int force_remove_file(file_t* file);

#endif
