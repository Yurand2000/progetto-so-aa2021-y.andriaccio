#include "server_compression.h"

#include <stdlib.h>
#include <string.h>
#include "../errset.h"
#include "../../minilzo/minilzo.h"

#define MEM_REQ(size) ( ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) )

static int compress;
static void* workmemory;

//returns 0 on continue, 1 on return, -1 on error and sets errno.
static int check_param(void* in, size_t in_size, void** out, size_t* out_size);
static int clone_data(void* in, size_t in_size, void** out, size_t* out_size);

int compression_init(int use_minilzo)
{
	if (use_minilzo) compress = 1;
	else compress = 0;

	if (compress)
	{
		if (lzo_init() != LZO_E_OK) return -1;

		/* allocate work memory needed for the compression algorithm
		 * the size used is the same as the example script. is it enough? */
		MALLOC(workmemory, MEM_REQ(LZO1X_1_MEM_COMPRESS));
	}
	return 0;
}

int compression_destroy()
{
	free(workmemory);
	return 0;
}

int compress_data(void* in, size_t in_size, void** out, size_t* out_size)
{
	int ret; ERRCHECK((ret = check_param(in, in_size, out, out_size)));
	if (ret == 1) return 0;

	if (compress)
	{
		/* the given size is to make sure the algorithm works correctly.
		 * it is used by the example script, not sure if it is enough or not.
		 * we give some more space just to prevent writing in bad memory. */
		MALLOC(*out, (in_size + in_size / 16 + 64 + 3));

		lzo_uint out_len;
		ret = lzo1x_1_compress(in, (lzo_uint)in_size, *out, &out_len, workmemory);
		if (ret == LZO_E_OK)
		{
			if (out_len >= (lzo_uint)in_size)
			{
				free(*out); *out = NULL;
				ERRSET(EILSEQ, -1);
			}
			else
			{
				*out_size = (size_t)out_len;
				REALLOC(*out, *out, *out_size);
				return 0;
			}
		}
		else
		{
			free(*out); *out = NULL;
			ERRSET(ENOTSUP, -1);
		}
	}
	else return clone_data(in, in_size, out, out_size);
}

int decompress_data(void* in, size_t in_size, void** out, size_t* out_size)
{
	int ret; ERRCHECK((ret = check_param(in, in_size, out, out_size)));
	if (ret == 1) return 0;

	if (compress)
	{
		/* the given size is to make sure the algorithm works correctly.
		* it is used by the example script, not sure if it is enough or not.
		* we give some more space just to prevent writing in bad memory. */
		MALLOC(*out, (in_size + in_size / 16 + 64 + 3));

		lzo_uint out_len;
		ret = lzo1x_decompress(in, in_size, *out, &out_len, NULL);
		if (ret == LZO_E_OK)
		{
			if (out_len < (lzo_uint)in_size)
			{
				free(*out); *out = NULL;
				ERRSET(EILSEQ, -1);
			}
			else
			{
				*out_size = (size_t)out_len;
				REALLOC(*out, *out, *out_size);
				return 0;
			}
		}
		else
		{
			free(*out); *out = NULL;
			ERRSET(ENOTSUP, -1);
		}
	}
	else return clone_data(in, in_size, out, out_size);
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