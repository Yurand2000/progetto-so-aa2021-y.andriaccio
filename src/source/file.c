#include "file.h"

#include <stdlib.h>
#include <string.h>

#include "errset.h"

#define LOCK(file) pthread_mutex_lock(&file->mutex)
#define UNLOCK(file) pthread_mutex_unlock(&file->mutex)

#define CHECK_OPEN_FILE(file) {\
	int opn = is_open_file(file);\
	if (opn == -1) { UNLOCK(file); return -1; }\
	if (opn == 1) ERRSETDO(EPERM, UNLOCK(file), -1);\
}

int init_file_struct(file_t* file)
{
	if(file == NULL) ERRSET(EINVAL, -1);
	file->owner = OWNER_NEXIST;
	pthread_mutex_init(&file->mutex, NULL);
	*(file->name) = '\0';
	file->data_size = 0;
	file->open_size = -1;
	file->data = NULL;
	file->open_data = NULL;
	return 0;
}

int destroy_file_struct(file_t* file)
{
	if (file == NULL) ERRSET(EINVAL, -1);
	pthread_mutex_destroy(&file->mutex);
	free(file->data);
	free(file->open_data);
	file->data = file->open_data = NULL;
	return 0;
}

int reset_file_struct(file_t* file)
{
	ERRCHECK(destroy_file_struct(file));
	ERRCHECK(init_file_struct(file));
	return 0;
}

int create_file_struct(file_t* file, char* filename, int owner)
{
	if(file == NULL || filename == NULL || owner < OWNER_NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	file->owner = owner;
	strncpy(file->name, filename, FILE_NAME_MAX_SIZE);
	UNLOCK(file);
	return 0;
}

int valid_file_struct(file_t const* file)
{
	if(file == NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	int own = file->owner;
	UNLOCK(file);

	if(own >= OWNER_NULL) return 0;
	else ERRSET(ENOENT, -1);
}

int check_file_name(file_t const* file, char const* name)
{
	if (file == NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	int cmp = strcmp(file->name, name);
	UNLOCK(file);

	if (cmp == 0) return 0;
	else return 1;
}

int is_open_file(file_t const* file)
{
	if (file == NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	ssize_t osize = file->open_size;
	UNLOCK(file);

	if (osize == -1) return 1;
	else return 0;
}

int open_file(file_t* file, int who)
{
	if (file == NULL || who <= OWNER_NULL) ERRSET(EINVAL, -1);

	LOCK(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EPERM, UNLOCK(file), -1);
	if (file->open_size > -1) { UNLOCK(file); return 0; }
	else
	{
		if (file->data_size == 0)
			file->open_size = 0;
		else
		{
			//decompress the file data into the open_data pointer.
			//compression not implemented yet.
			file->open_size = file->data_size;
			MALLOC(file->open_data, file->open_size);
			memcpy(file->open_data, file->data, file->data_size);
		}

		UNLOCK(file);
		return 0;
	}
}

int close_file(file_t* file, int who)
{
	if (file == NULL || who <= OWNER_NULL) ERRSET(EINVAL, -1);

	LOCK(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EPERM, UNLOCK(file), -1);	
	if (file->open_size == -1) { UNLOCK(file); return 0; }
	else
	{
		//compress the file open_data into the data pointer.
		//compression not implemented yet.
		if (file->open_size > 0)
		{
			file->data_size = file->open_size;
			REALLOC(file->data, file->data, file->data_size);
			memcpy(file->data, file->open_data, file->data_size);
		}

		free(file->open_data);
		file->open_size = -1;
		UNLOCK(file);
		return 0;
	}
}

int read_file(file_t* file, int who, void** out_data, size_t* out_data_size)
{
	if(file == NULL || who <= OWNER_NULL ||
		out_data == NULL || out_data_size == NULL) ERRSET(EINVAL, -1);

	LOCK(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EPERM, UNLOCK(file), -1);
	CHECK_OPEN_FILE(file);
	if(*out_data_size <= file->open_size)
	{
		void* out;
		REALLOCDO(out, *out_data, file->open_size, { UNLOCK(file); free(*out_data); });
		*out_data = out;
		*out_data_size = file->open_size;
	}

	memcpy(*out_data, file->open_data, file->open_size);
	UNLOCK(file);
	return 0;
}
 
int write_file(file_t* file, int who, const void* data, size_t data_size)
{
	if(file == NULL || who <= OWNER_NULL ||
		data == NULL || data_size == 0) ERRSET(EINVAL, -1);

	LOCK(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EPERM, UNLOCK(file), -1);
	CHECK_OPEN_FILE(file);
	if(file->open_size != 0) ERRSET(EEXIST, -1);

	MALLOCDO(file->open_data, data_size, UNLOCK(file));
	file->open_size = data_size;
	memcpy(file->open_data, data, data_size);
	UNLOCK(file);
	return 0;
}

int append_file(file_t* file, int who, const void* data, size_t data_size)
{
	if(file == NULL || who <= OWNER_NULL ||
		data == NULL || data_size == 0) ERRSET(EINVAL, -1);

	LOCK(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EPERM, UNLOCK(file), -1);
	CHECK_OPEN_FILE(file);
	if(file->open_size == 0) ERRSETDO(ENOENT, UNLOCK(file), -1);

	void* out;
	REALLOCDO(out, file->open_data, file->open_size + data_size, UNLOCK(file));
	file->open_data = out;

	memcpy((char*)file->open_data + file->open_size, data, data_size);
	file->open_size += data_size;
	UNLOCK(file);
	return 0;
}

int lock_file(file_t* file, int who)
{
	if(file == NULL || who <= OWNER_NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EPERM, UNLOCK(file), -1);

	file->owner = who;
	UNLOCK(file);
	return 0;
}

int unlock_file(file_t* file, int who)
{
	if(file == NULL || who <= OWNER_NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EPERM, UNLOCK(file), -1);

	file->owner = OWNER_NULL;
	UNLOCK(file);
	return 0;
}

int remove_file(file_t* file, int who)
{
	if(file == NULL || who < OWNER_NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EPERM, UNLOCK(file), -1);
	UNLOCK(file);

	return reset_file_struct(file);	
}
