#include "block_system.h"

#include "../errset.h"

int init_table(table* tbl, void* memory, size_t block_size, size_t block_count)
{
	if(table == NULL || memory == NULL) ERRSET(EINVAL, -1);
	table->table = memory;
	table->block_size = block_size;
	table->block_count = block_count;
	return 0;
}

int get_table_block(table* tbl, size_t block_num, block* block_out)
{
	if(tbl == NULL || block_out == NULL || block_num > tbl->block_count) ERRSET(EINVAL, -1);
	*block_out = &(tbl->table[block_num]);
	return 0;
}

size_t get_block_size(table* tbl)
{
	return tbl->block_size;
}

endif
