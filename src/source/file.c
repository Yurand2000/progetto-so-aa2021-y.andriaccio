#include "file.h"

#include "../errset.h"

int init_file_struct(file_t* file)
{
	if(file == NULL) ERRSET(EINVAL, -1);
	file->owner = OWNER_NEXIST;
	*(file->name) = '\0';
	file->data_size = 0;
	file->data = NULL;
	return 0;
}

int reset_file_struct(file_t* file)
{
	if(file == NULL) ERRSET(EINVAL, -1);
	free(file->data);
	init_file_struct(file);	
}

int create_file_struct(file_t* file, char* filename, int owner)
{
	if(file == NULL || filename == NULL || owner < OWNER_NULL) ERRSET(EINVAL, -1);
	file->owner = owner;
	strncpy(file->name, filename, FILE_NAME_MAX_SIZE);
	return 0;
}

int valid_file_struct(file_t* file)
{
	if(file == NULL) ERRSET(EINVAL, -1);
	if(file->owner >= OWNER_NULL) return 0;
	else ERREST(ENOENT, -1);
}

int read_file(file_t* file, int who, void** out_data, size_t* out_data_size)
{
	if(file == NULL || who <= OWNER_NULL ||
		out_data == NULL || out_data_size == NULL) ERRSET(EINVAL, -1);
	if(file->owner != OWNER_NULL && file->owner != who) ERRSET(EPERM, -1);

	if(*out_data_size <= file->data_size)
	{
		void* out;
		REALLOCDO(out, *out_data, file->data_size, free(*out_data));
		*out_data = out;
		*out_data_size = file->data_size;
	}

	memcpy(*out_data, file->data, file->data_size);
	return 0;
}
 
int write_file(file_t* file, int who, const void* data, size_t data_size)
{
	if(file == NULL || who <= OWNER_NULL ||
		data == NULL || data_size == NULL) ERRSET(EINVAL, -1);
	if(file->owner != OWNER_NULL && file->owner != who) ERRSET(EPERM, -1);
	if(file->data_size != 0) ERRSET(EEXIST, -1);

	MALLOC(file->data, data_size);
	file->data_size = data_size;
	memcpy(file->data, data, data_size);
	return 0;
}

int append_file(file_t* file, int who, const void* data, size_t data_size)
{
	if(file == NULL || who <= OWNER_NULL ||
		data == NULL || data_size == NULL) ERRSET(EINVAL, -1);
	if(file->owner != OWNER_NULL && file->owner != who) ERRSET(EPERM, -1);
	if(file->data_size == 0) ERRSET(ENOENT, -1);

	void* out;
	REALLOC(out, file->data, file->data_size + data_size);
	file->data = out;

	memcpy(file->data + file->data_size, data, data_size);
	file->data_size + data_size;
	return 0;
}

int lock_file(file_t* file, int who)
{
	if(file == NULL || who <= OWNER_NULL) ERRSET(EINVAL, -1);
	if(file->owner != OWNER_NULL && file->owner != who) ERRSET(EPERM, -1);

	file->owner = who;
	return 0;
}

int unlock_file(file_t* file, int who)
{
	if(file == NULL || who <= OWNER_NULL) ERRSET(EINVAL, -1);
	if(file->owner != OWNER_NULL && file->owner != who) ERRSET(EPERM, -1);

	file->owner = OWNER_NULL;
	return 0;
}

int remove_file(file_t* file, int who)
{
	if(file == NULL || who < OWNER_NULL) ERRSET(EINVAL, -1);
	if(file->owner != OWNER_NULL && file->owner != who) ERRSET(EPERM, -1);

	return reset_file_struct(file);	
}
