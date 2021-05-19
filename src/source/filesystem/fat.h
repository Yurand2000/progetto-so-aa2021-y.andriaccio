#ifndef FILESYSTEM_FAT_TABLE
#define FILESYSTEM_FAT_TABLE

#include <stdlib.h>

#define BLOCK_END 0
#define BLOCK_EMPTY -1
#define BLOCK_SYSTEM -2

typedef int* fat_t;

/* initializes a fat-like table, using table arg as memory, of table_size blocks.
 * sets the whole table to zeros (does not mean the table is empty if all zeros)..
 * calling then reset_fat will set the filesystem reserved blocks to used.
 * returns 0 on success, -1 on error and sets errno. */
int init_fat(fat_t** table, void* memory, size_t table_size);

/* resets the table to all zeros and sets the first (reserved_blocks) blocks as
 * used and reserved to the filesystem. */
int reset_fat(fat_t** table, size_t reserved_blocks);

/* sets the pointer to the next block
 * returns 0 on success, -1 on error and sets errno. */
int set_block(fat_t** table, int block, int next_block);

/* sets the given block as empty.
 * returns 0 on success, -1 on error and sets errno. */
int reset_block(fat_t** table, int block);

/* gets the next block from the given block
 * return 0 on success, -1 on error and sets errno. */
int get_block(fat_t** table, int block, int* next_block);

/* finds the first empty block, returning its index in (block).
 * if (lastprt) != NULL, searches the next empty block starting from the
 * last searched position. sets next ptr to the newly found block.
 * returns 0 on success, -1 on error and sets errno. */
int find_empty_block(fat_t** table, size_t table_size, int* block,
	size_t** lastptr, int prec);

#endif
