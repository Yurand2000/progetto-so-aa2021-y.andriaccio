#ifndef SERVER_COMPRESSION
#define SERVER_COMPRESSION

#include <stdlib.h>

//all functions return 0 on success, -1 on failure setting errno.

//initialize the compression system.
int compression_init(int use_minilzo);
int compression_destroy();

/* tries to compress and decompress the data. returns 0 on successful compression,
 * returns 1 on copy_data only (compression fail), -1 on error. [in] pointer is
 * never invalidated. if mode == 1, it will try compress, else will just copy the data. */
int compress_data(void* in, size_t in_size, void** out, size_t* out_size);
int decompress_data(void* in, size_t in_size, void** out, size_t* out_size, int mode);

#endif