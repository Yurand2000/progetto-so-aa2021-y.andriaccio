#include "server_compression.h"

#include <stdlib.h>
#include <string.h>
#include "../errset.h"

//returns 0 on continue, 1 on return, -1 on error and sets errno.
static int check_param(void* in, size_t in_size, void** out, size_t* out_size);
static int clone_data(void* in, size_t in_size, void** out, size_t* out_size);

int compression_init()
{
	return 0;
}

int compression_destroy()
{
	return 0;
}

int compress_data(void* in, size_t in_size, void** out, size_t* out_size)
{
	int ret; ERRCHECK((ret = check_param(in, in_size, out, out_size)));
	if (ret == 1) return 0;

	return clone_data(in, in_size, out, out_size);
}

int decompress_data(void* in, size_t in_size, void** out, size_t* out_size, int mode)
{
	int ret; ERRCHECK((ret = check_param(in, in_size, out, out_size)));
	if (ret == 1) return 0;

	return clone_data(in, in_size, out, out_size);
}

static int check_param(void* in, size_t in_size, void** out, size_t* out_size)
{
	if (out == NULL || out_size == NULL) ERRSET(EINVAL, -1);
	if (in == NULL && in_size == 0)
	{
		*out = NULL;
		*out_size = 0;
		return 1;
	}
	else if (in == NULL && in_size != 0) ERRSET(EINVAL, -1);
	return 0;
}

static int clone_data(void* in, size_t in_size, void** out, size_t* out_size)
{
	*out_size = in_size;
	MALLOC(*out, *out_size);
	memcpy(*out, in, *out_size);
	return 0;
}