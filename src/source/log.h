#ifndef LOG_FILE
#define LOG_FILE
#include "win32defs.h"

#define FILE_NAME_MAX_SIZE 256

#include <pthread.h>

typedef struct log_file
{
	char filename[FILE_NAME_MAX_SIZE];
	pthread_mutex_t mux;
} log_t;

/* all functions return 0 on success, -1 on failure setting errno. */

int init_log_file_struct(log_t* log, char* filename);
int destroy_log_file_struct(log_t* log);

int reset_log(log_t* log);
int print_line_to_log_file(log_t* log, char* line);

#endif