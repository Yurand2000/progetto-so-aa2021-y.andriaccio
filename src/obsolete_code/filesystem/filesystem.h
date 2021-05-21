#ifndef FILESYSTEM_STRUCT
#define FILESYSTEM_STRUCT

#include <stdlib.h>

#include "block_system.h"
#include "fat.h"
#include "file.h"

typedef struct filesystem_struct {
	table blocks;
	fat_t table;
	file_t* files;
	size_t mem_size;
	size_t block_size;
	size_t file_count;
} fs_t;

//FILESYSTEM GENERIC

/* initializes the given memory (memory and mem_size args) into a filesystem.
 * the fs argument is a pointer to the fs structure, since it is stored into
 * the filesystem itself (as all the other structures!). it requires also the
 * size of each block and the number of max files supported. It is a very crude
 * implementation of a file system using blocks, having all the file info tables
 * already allocated in memory, which is very memory inconvenient, but simple.
 * returns 0 on success or -1 in case of error, sets errno.
 * (EINVAL if invalid args, others for internal errors) */
int init_filesystem(fs_t** fs, void* memory, size_t mem_size,
	size_t blk_size, size_t file_count);

/* resets an initialized filesystem structure to its default state, as it was
 * just initialized. It will use the same configuration as it was initialized.
 * returns 0 on success or -1 in case of error, sets errno.
 * (EINVAL if invalid args, others for internal errors) */
int reset_filesystem(fs_t* fs);

//FILE OPERATIONS

/* finds the file with the given file name and returns its id if present.
 * returns 0 on success, -1 on error and sets errno.
 * (ENOENT if file not found, EINVAL if invalid args, others for internal errors) */
int find_file(fs_t* fs, char* filename, int* fileid);

/* creates an empty file with the given file name, sets the given owner (0 if no owner)
 * and returns its id. If already present returns error.
 * returns 0 on success, -1 on error and sets errno.
 * (EEXIST if file exists, ENFILE if there are no file slots available,
 * EINVAL if invalid args, others for internal errors) */
int create_file(fs_t* fs, char* filename, int who, int* fileid);

/* removes the given file from the filesystem. May fail if the file is locked.
 * returns 0 on success, -1 on error and sets errno. */
int delete_file(fs_t* fs, int fileid, int who);

/* reads the whole file content into the data pointer. the data pointer should
 * be NULL or a pointer allocated with malloc/calloc/realloc. Any data present
 * will be deleted. Sets datasize with the data pointer size. It might be real-
 * located for more space. It may fail if the file is locked. On file locked the
 * data structure will be untouched and valid. the datasize will be unchanged.
 * returns 0 on success, -1 on error and sets errno. */
int read_file(fs_t* fs, int fileid, int who, void** data, size_t* datasize);

/* writes the given data to te given file, if overwrite is set or if the file
 * has no blocks, it will write the file, otherwise will fail. If the file is
 * locked it may fail.
 * returns 0 on success, -1 on error and sets errno. */
int write_file(fs_t* fs, int fileid, int who, void* data, size_t datasize, int overwrite);

/* appends the given data to the given file. It may fail if the file is locked.
 * returns 0 on success, -1 on error and sets errno. */
int append_file(fs_t* fs, int fileid, int who, void* data, size_t datasize);

/* gets the current owner of the given file.
 * returns 0 on success, -1 on error and sets errno. */
int get_owner(fs_t* fs, int fileid, int* owner);

/* sets the owner of the given file. Will fail if the file is locked by someone.
 * returns 0 on success, -1 on error and sets errno. */
int set_owner(fs_t* fs, int fileid, int who);

/* resets the owner of the given file. Will fail if the file is locked by someone.
 * returns 0 on success, -1 on error and sets errno. */
int reset_owner(fs_t* fs, int fileid, int who);

#endif
