#ifndef GENERIC_DATABUFFER
#define GENERIC_DATABUFFER

#include <stdlib.h>

typedef struct data_buffer {
	void* buffer;
	size_t buf_size;
	size_t buf_cap;
} databuf;

/* all calls return 0 on success and -1 on error, setting errno.
 * if errno == ENOMEM the state of the data_buf structure is lost.
 * else other errors are not fatal. */

//initializes a data_buffer structure
//don't forget to delete when done calling destroy_data_buffer
//returns 0 on success, -1 on error and sets errno
int create_data_buffer(databuf* out);

//pops from the back of the buffer datalen bytes, writing them into read_data
//returns 0 on success, -1 on error and sets errno
int read_buf(databuf* buf, size_t datalen, void* read_data);

//pushes to the back of the buffer datalen bytes, reading them from write_data
//returns 0 on success, -1 on error and sets errno
int write_buf(databuf* buf, size_t datalen, const void* write_data);

//destroys a data_buffer structure
//returns 0 on success, -1 on error and sets errno
int destroy_data_buffer(databuf* buf);

/* resizes the data buffer, allowing writing using a raw pointer
 * if newsize is smaller than buf_size, some of the current data will be lost
 * returns 0 on success, -1 on error and sets errno */
int resize_data_buffer(databuf* buf, size_t newsize);

/* replaces the data into the data buffer with the given buffer.
 * useful if you want to link an existing buffer into this structure.
 * the old buffer is destroyed. make sure that buffer has been allocated
 * through malloc/calloc/realloc, otherwise the state of the databuf structure
 * is undefined after this function call. 
 * returns 0 on success, -1 on error and sets errno. */
int replace_data_buffer(databuf* buf, void* buffer, size_t buf_size, size_t buf_cap);

#endif
