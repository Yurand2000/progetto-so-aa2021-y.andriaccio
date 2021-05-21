#ifndef FILESYSTEM_FILE
#define FILESYSTEM_FILE

#include <stdlib.h>

#define OWNER_NEXIST -1
#define OWNER_NULL 0
#define FILE_NAME_MAX_SIZE 256

typedef struct file_struct {
	int owner;
	char name[FILE_NAME_MAX_SIZE];
	size_t data_size;
	void* data;
} file_t;

/* initializes the file struct. it sets as a non existing file.
 * returns 0 on success, -1 on error and sets errno. */
int init_file_struct(file_t* file);

/* resets the file struct. frees allocated memory.
 * return 0 on success, -1 on error and sets errno. */
int reset_file_struct(file_t* file);

/* initializes the file struct with an empty file.
 * returns 0 on success, -1 on error and sets errno. */
int create_file_struct(file_t* file, char* filename, int owner);

/* checks if the structure contains an existing file. If errno is set to ENOENT,
 * there was no file. returns 0 on success, -1 on error and sets errno. */
int valid_file_struct(file_t* file);

/* read the whole file into the given buffer, reallocating if necessary.
 * returns 0 on success, -1 on error and sets errno. */
int read_file(file_t* file, int who, void** out_data, size_t* out_data_size);

/* writes the whole file from the given buffer.
 * returns 0 on success, -1 on error and sets errno. */
int write_file(file_t* file, int who, const void* data, size_t data_size);

/* appends the given data to the file.
 * returns 0 on success, -1 on error and sets errno. */
int append_file(file_t* file, int who, const void* data, size_t data_size);

/* sets the owner to a given file.
 * returns 0 on success, -1 on error and sets errno. */
int lock_file(file_t* file, int who);

/* unsets the owner to a given file.
 * returns 0 on success, -1 on error and sets errno. */
int unlock_file(file_t* file, int who);

/* deletes the given file.
 * returns 0 on success, -1 on error and sets errno. */
int remove_file(file_t* file, int who);

#endif
