#include "databuf.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "errset.h"

#define REALLOC_SIZE 2

//reallocating buffer for additional size, hidden function
//sets errno on error, returns -1.
static int realloc_buf(databuf* buf, size_t newsize)
{
	buf->buffer = realloc(buf->buffer, newsize);
	if(buf->buffer == NULL && newsize != 0)
		return -1;
	else
	{
		buf->buf_cap = newsize;
		return 0;
	}
}

int create_data_buffer(databuf* out)
{
	if(out == NULL) ERRSET(EINVAL, -1);
	
	out->buffer = NULL;
	out->buf_size = 0;
	out->buf_cap = 0;
	return 0;
}

int read_buf(databuf* buf, size_t datalen, void* read_data)
{
	if(buf == NULL || read_data == NULL) ERRSET(EINVAL, -1);
	if(datalen > buf->buf_size) ERRSET(ENOBUFS, -1);

	buf->buf_size -= datalen;
	memcpy(read_data, buf->buffer + buf->buf_size, datalen);
	//reallocation check? (if capacity > 2 * size then capacity /= 2)
	//not implemented.
	return 0;
}

int write_buf(databuf* buf, size_t datalen, const void* write_data)
{
	if(buf == NULL || write_data == NULL) ERRSET(EINVAL, -1);

	if(buf->buf_cap - buf->buf_size < datalen)
	{
		if(realloc_buf(buf, buf->buf_cap * REALLOC_SIZE) == -1)
			return -1;
	}

	memcpy(buf->buffer + buf->buf_size, write_data, datalen);
	buf->buf_size += datalen;
	return 0;
}

int destroy_data_buffer(databuf* buf)
{
	if(buf == NULL) ERRSET(EINVAL, -1);

	free(buf->buffer);
	buf->buf_size = 0;
	buf->buf_cap = 0;
	return 0;
}

int resize_data_buffer(databuf* buf, size_t newsize)
{
	if(buf == NULL) ERRSET(EINVAL, -1);

	int err = realloc_buf(buf, newsize);
	if(err == -1) return -1;

	if(buf->buf_size > newsize)
		buf->buf_size = newsize;
	return 0;
}

int replace_data_buffer(databuf* buf, void* buffer, size_t buf_size, size_t buf_cap)
{
	if(buf == NULL || (buffer == NULL && buf_cap != 0) || buf_size > buf_cap)
		ERRSET(EINVAL, -1);

	free(buf->buffer);
	buf->buffer = buffer;
	buf->buf_size = buf_size;
	buf->buf_cap = buf_cap;
	return 0;
}
