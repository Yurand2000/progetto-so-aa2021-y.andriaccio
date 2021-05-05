#ifndef GENERIC_HASH
#define GENERIC_HASH

#include <stdint.h>

/* this hash function is named djb2, after Daniel J. Bernstein, inventor of this function.
 * this is a sligtly modified version using fixed size types in C (stdint.h library)
 * it accepts a generic array instead of a null terminated string, which can hopefully
 * return nice hashes of any data it is passed to.
 *   NO checks are performed upon the data passed!
 *   Please avoid using invalid data pointers and/or sizes.
 * */
uint64_t hash(void* data, size_t len)
{
	uint64_t hash = 5381;
	for(size_t i = 0; i < len; i++)
		hash = ((hash << 5) + hash) + data[i]; /* hash * 33 + data[i] */

	return hash;
}

//here is reproduced the original code
/* unsigned long
 * hash(unsigned char *str)
 * {
 *	unsigned long hash = 5381;
 *	int c;
 *
 *	while(c = *str++)
 *		hash = ((hash << 5) + hash) + c;
 *
 *	return hash;
 * } */

#endif
