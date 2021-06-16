#ifndef LOG_FILE
#define LOG_FILE
#include "win32defs.h"

#define FILENAME_MAX_SIZE 256

#include <pthread.h>

typedef struct log_file
{
	char filename[FILENAME_MAX_SIZE];
	pthread_mutex_t mux;
} log_t;

/* initializes the log structure with the given file name.
 * returns 0 on success, -1 on failure and sets errno. */
int init_log_file_struct(log_t* log, char* logname);

/* destroyes the log structure.
 * returns 0 on success, -1 on failure and sets errno. */
int destroy_log_file_struct(log_t* log);

/* resets the log given its struct.
 * returns 0 on success, -1 on failure and sets errno. */
int reset_log(log_t* log);

/* appends a line with the given text to the log. line must be null terminated.
 * returns 0 on success, -1 on failure and sets errno. */
int print_line_to_log_file(log_t* log, char* line);

#endif