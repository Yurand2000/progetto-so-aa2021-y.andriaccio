#include "file.h"

#include "../errset.h"

int init_file_struct(file_t* file)
{
	if(file == NULL) ERRSET(EINVAL, -1);
	file->owner = -1;
	*(file->name) = '\0';
	file->block_count = 0;
	file->file_size = 0;
	file->first_block = -1;
	file->last_block = -1;
	return 0;
}

int create_file_struct(file_t* file, char* filename, int owner)
{
	if(file == NULL || filename == NULL || owner < 0) ERRSET(EINVAL, -1);
	file->owner = owner;
	strncpy(file->name, filename, FILE_NAME_MAX_SIZE);
	return 0;
}

int valid_file_struct(file_t* file)
{
	if(file == NULL) ERRSET(EINVAL, -1);
	if(file->owner >= 0) return 0;
	else ERREST(ENOENT, -1);
}
