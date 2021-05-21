#ifndef FILESYSTEM_BLOCK_TABLE
#define FILESYSTEM_BLOCK_TABLE

#include <stdlib.h>

typedef char block;

typedef struct block_table
{
	block* table;
	size_t block_size;
	size_t block_count;
} table;

//table operations

/* inits the block_table with a number of blocks the given block_size.
 * all the blocks should be allocated on contigous memory.
 * returns 0 on success, -1 on error and sets errno. */
int init_table(table* tbl, void* memory, size_t block_size, size_t block_count);

/* gets a specific block from the block_table.
 * returns 0 on success, -1 on error and sets errno. */
int get_table_block(table* tbl, size_t block_num, block* block_out);

/* returns the size of each block in bytes */
size_t get_block_size(table* tbl);

endif
