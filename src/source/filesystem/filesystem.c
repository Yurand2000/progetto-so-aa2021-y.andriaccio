#include "filesystem.h"

#include "errset.h"

/* ceiling rounding for integer operands:
 * stack overflow question 2745074 */
#define INT_CEIL(x, y) (1 + ((x-1) / y))

int init_filesystem(fs_t** fs, void* memory, size_t mem_size,
	size_t blk_size, size_t file_count)
{
	if(fs == NULL || memory == NULL) ERRSET(EINVAL, -1);
	*fs = memory;

	fs->mem_size = mem_size;
	fs->block_size = blk_size;
	fs->file_count = file_count;

	if( init_table(&(fs->blocks), memory, blk_size, mem_size / blk_size) != 0) return -1;

	//memory + sizeof(fs_t) is the offset for the fat table storage.
	if( init_fat(&(fs->table), memory + sizeof(fs_t), fs->block_size) != 0) return -1;

	//offset for the files array.
	fs->files = memory + sizeof(fs_t) + sizeof(int) * fs-> block_size;
	for(int i = 0; i < fs->file_count; i++)
	{
		if( init_file_struct(&(fs->files[i])) != 0) return -1;
	}

	if( reset_fat(&(fs->table), INT_CEIL((sizeof(fs_t) + sizeof(int) * fs->block_size
			+ sizeof(file_t) * fs->file_count)
		)) != 0 ) return -1;
	return 0;
}

int reset_filesystem(fs_t* fs)
{
	if(fs == NULL) ERRSET(EINVAL, -1);

	if( reset_fat(&(fs->table), INT_CEIL((sizeof(fs_t) + sizeof(int) * fs->block_size
			+ sizeof(file_t) * fs->file_count)
		)) != 0 ) return -1;

	for(int i = 0; i < fs->file_count; i++)
	{
		if( reset_file_struct(&(fs->files[i])) != 0) return -1;
	}
	return 0;
}

int find_file(fs_t* fs, char* filename, int* fileid)
{
	if(fs == NULL || filename == NULL || fileid == NULL) ERRSET(EINVAL, -1);
	for(int i = 0; i < fs->file_count; i++)
	{
		if( valid_file_struct( &(fs->files[i]) ) == 0 )
		{
			if( strcmp(filename, fs->files[i].name) == 0 )
			{
				*fileid = i;
				return 0;
			}
		}
		else if(errno != ENOENT) return -1;
	}
	ERRSET(ENOENT, -1);
}

int create_file(fs_t* fs, char* filename, int who, int* fileid)
{
	if(fs == NULL || filename == NULL || fileid == NULL) ERRSET(EINVAL, -1);
	int findfile;
	if( find_file(fs, filename, &findfile) == 0 )
		ERRSET(EEXIST, -1);
	else if( errno == ENOENT )
	{
		for(int i = 0; i < fs->file_count; i++)
		{
			if(valid_file_struct( &(fs->files[i]) ) != 0)
			{
				if(errno == ENOENT)
				{
					*fileid = i;
					return create_file_struct( &(fs->files[i]),
						filename, who );
				}
				else return -1;
			}
		}
		ERRSET(ENFILE, -1);
	}
	else return -1;
}

int delete_file(fs_t* fs, int fileid, int who)
{
	if(fs == NULL || fileid > fs->file_count) ERRSET(EINVAL, -1);
	if( valid_file_struct( &(fs->files[fileid]) ) == 0)
	{
		int owner = (fs->files[fileid])->owner;
		if(owner != 0 && owner != who) ERRSET(EACCES, -1);

		int block = (fs->files[fileid])->first_block;
		int next_block;
		while(block != BLOCK_END)
		{
			ERRCHECK(get_block(fs->table, block, &next_block), -1);
			ERRCHECK(reset_block(fs->table, block), -1);
			block = next_block;
		}

		return reset_file_struct( &(fs->files[fileid]) );
	}
	else return -1;
}

int read_file(fs_t* fs, int fileid, int who, void** data, size_t* datasize)
{

}

int write_file(fs_t* fs, int fileid, int who, void* data, size_t datasize, int overwrite)
{

}

int append_file(fs_t* fs, int fileid, int who, void* data, size_t datasize)
{

}

int get_owner(fs_t* fs, int fileid, int* owner)
{

}

int set_owner(fs_t* fs, int fileid, int who)
{

}

int reset_owner(fs_t* fs, int fileid, int who)
{

}
