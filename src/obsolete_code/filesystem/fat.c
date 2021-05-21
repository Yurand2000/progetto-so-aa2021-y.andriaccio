#include "fat.h"

#include "../errest.h"

int init_fat(fat_t** table, void* memory, size_t table_size)
{
	if(table == NULL || memory == NULL) ERRSET(EINVAL, -1);
	*table = memory;
	memset(*table, 0, sizeof(int) * table_size);
	return 0;
}

int reset_fat(fat_t** table, size_t reserved_blocks)
{
	if(table == NULL) ERRSET(EINVAL, -1);
	for(size_t i = 0; i < reserved_blocks; i++) (*table)[i] = BLOCK_SYSTEM;
	for(size_t i = reserved_blocks; i < table_size; i++) (*table)[i] = BLOCK_EMPTY;
	return 0;
}

int set_block(fat_t** table, size_t block, int next_block)
{
	if(table == NULL || next_block < 0) ERRSET(EINVAL, -1);
	(*table)[block] = next_block;
}

int reset_block(fat_t** table, size_t block)
{
	if(table == NULL) ERRSET(EINVAL, -1);
	(*table)[block] = BLOCK_EMPTY;
}

int find_empty_block(fat_t** table, size_t table_size, size_t* block,
	size_t** lastptr, size_t prec)
{
	if(table == NULL || block == NULL || prec > table_size) ERRSET(EINVAL, -1);
	if( (*table)[prec] != BLOCK_END ) ERRSET(EINVAL, -1);

	size_t start;
	if(lastptr != NULL) start = lastptr;
	else start = 0;

	for(size_t i = 0; i < table_size; i++)
	{
		if( (*table)[start] == BLOCK_EMPTY )
		{
			(*table)[start] = BLOCK_END;
			(*table)[prec] = start;
			if( lastprt != NULL )
				*lastptr = (start + 1) % table_size;
			*block = start;
			return 0;
		}
		start++;
		if( start >= table_size ) start = 0;
	}
	ERRSET(ENOMEM, -1);
}

#endif
