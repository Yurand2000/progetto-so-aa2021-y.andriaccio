#include "databuf.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "errset.h"

#define REALLOC_ADDER 10
#define REALLOC_MULTIPLIER 2

//reallocating buffer for additional size, static function
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

int pop_buf(databuf* buf, size_t datalen, void* read_data)
{
	if(buf == NULL || read_data == NULL) ERRSET(EINVAL, -1);
	if(datalen > buf->buf_size) ERRSET(ENOBUFS, -1);

	buf->buf_size -= datalen;
	memcpy(read_data, (char*)buf->buffer + buf->buf_size, datalen);
	return 0;
}

int pop_buf_discard(databuf* buf, size_t datalen)
{
	if (buf == NULL) ERRSET(EINVAL, -1);
	if (datalen > buf->buf_size) ERRSET(ENOBUFS, -1);

	buf->buf_size -= datalen;
	return 0;
}

int push_buf(databuf* buf, size_t datalen, const void* write_data)
{
	if(buf == NULL || write_data == NULL) ERRSET(EINVAL, -1);

	while(buf->buf_cap - buf->buf_size < datalen)
		ERRCHECK(realloc_buf(buf, buf->buf_cap * REALLOC_MULTIPLIER + REALLOC_ADDER));

	memcpy((char*)buf->buffer + buf->buf_size, write_data, datalen);
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

	ERRCHECK(realloc_buf(buf, newsize));

	if(buf->buf_size > newsize)
		buf->buf_size = newsize;
	return 0;
}
