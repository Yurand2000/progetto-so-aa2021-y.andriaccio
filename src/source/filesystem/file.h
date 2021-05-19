#ifndef FILESYSTEM_FILE
#define FILESYSTEM_FILE

#include <stdlib.h>

#define FILE_NAME_MAX_SIZE 256

typedef struct file_struct {
	int owner;
	char name[FILE_NAME_MAX_SIZE];
	size_t block_count;
	size_t file_size;
	int first_block;
	int last_block;
} file_t;

/* initializes the file struct. it sets as a non existing file.
 * returns 0 on success, -1 on error and sets errno. */
int init_file_struct(file_t* file);

/* resets the file struct. it is the same as init_file_struct. */
#define reset_file_struct(file) init_file_struct(file)

/* initializes the file struct with an empty file.
 * returns 0 on success, -1 on error and sets errno. */
int create_file_struct(file_t* file, char* filename, int owner);

/* checks if the structure contains an existing file. If errno
 * is set to ENOENT, there was no file.
 * returns 0 on success, -1 on error and sets errno. */
int valid_file_struct(file_t* file);

#endif
