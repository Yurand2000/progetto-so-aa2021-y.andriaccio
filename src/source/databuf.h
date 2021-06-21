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

int create_data_buffer(databuf* out);
int destroy_data_buffer(databuf* buf);

int pop_buf(databuf* buf, size_t datalen, void* read_data);
int push_buf(databuf* buf, size_t datalen, const void* write_data);

int resize_data_buffer(databuf* buf, size_t newsize);

#endif
