#ifndef SERVER_COMPRESSION
#define SERVER_COMPRESSION

#include <stdlib.h>

//all functions return 0 on success, -1 on failure setting errno.

//initialize the compression system.
int compression_init(int use_minilzo);
int compression_destroy();

//in pointer is never invalidated.
int compress_data(void* in, size_t in_size, void** out, size_t* out_size);
int decompress_data(void* in, size_t in_size, void** out, size_t* out_size);

#endif