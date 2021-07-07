#include "file.h"

#include <stdlib.h>
#include <string.h>

#include "errset.h"

#define LOCK(file) ERRCHECK(pthread_mutex_lock(&file->mutex))
#define UNLOCK(file) ERRCHECK(pthread_mutex_unlock(&file->mutex))

#define CHECK_EXISTENCE(file) {\
	if (file->owner == OWNER_NEXIST) ERRSETDO(ENOENT, UNLOCK(file), -1);\
}

#define CHECK_OPEN_FILE(file) {\
	int opn = is_open_file_nolock(file);\
	if (opn == -1) { UNLOCK(file); return -1; }\
	if (opn == 1) ERRSETDO(EPERM, UNLOCK(file), -1);\
}

int init_file_struct(file_t* file)
{
	if(file == NULL) ERRSET(EINVAL, -1);
	file->owner = OWNER_NEXIST;

	pthread_mutexattr_t attr;
	ERRCHECK(pthread_mutexattr_init(&attr));
	ERRCHECK(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK));
	ERRCHECK(pthread_mutex_init(&file->mutex, &attr));
	ERRCHECK(pthread_mutexattr_destroy(&attr));
	file->name[0] = '\0';
	file->data_size = 0;
	file->open_size = -1;
	file->new_size = 0;
	file->data = NULL;
	file->open_data = NULL;

	file->fifo_creation_time = 0;
	file->lfu_frequency = 0;
	file->lru_clock = 0;
	return 0;
}

int destroy_file_struct(file_t* file)
{
	if (file == NULL) ERRSET(EINVAL, -1);
	ERRCHECK(pthread_mutex_destroy(&file->mutex));
	free(file->data);
	free(file->open_data);
	file->data = file->open_data = NULL;
	return 0;
}

int reset_file_struct(file_t* file)
{
	if (file == NULL) ERRSET(EINVAL, -1);

	free(file->data);
	free(file->open_data);

	file->owner = OWNER_NEXIST;
	file->name[0] = '\0';
	file->data_size = 0;
	file->open_size = -1;
	file->new_size = 0;
	file->data = NULL;
	file->open_data = NULL;

	file->fifo_creation_time = 0;
	file->lfu_frequency = 0;
	file->lru_clock = 0;
	return 0;
}

int create_file_struct(file_t* file, char const* filename, int owner)
{
	if(file == NULL || filename == NULL || owner <= OWNER_NEXIST) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	file->owner = owner;
	file->fifo_creation_time = time(NULL);
	file->lru_clock = 1;
	file->lfu_frequency = 1;
	strncpy(file->name, filename, FILE_NAME_MAX_SIZE);
	UNLOCK(file);
	return 0;
}

int is_existing_file(file_t* file)
{
	if(file == NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	int own = file->owner;
	UNLOCK(file);

	if(own >= OWNER_NULL) return 0;
	else return 1;
}

int get_file_name(file_t* file, char** out_data, size_t* out_data_size, size_t* read_size)
{
	if (file == NULL || out_data == NULL || out_data_size == NULL || read_size == NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	size_t name_len = strlen(file->name) + 1;
	if (*out_data_size < name_len)
	{
		void* out;
		REALLOCDO(out, *out_data, name_len, { UNLOCK(file); });
		*out_data = out;
		*out_data_size = name_len;
	}
	*read_size = name_len;
	memcpy(*out_data, file->name, name_len);
	UNLOCK(file);
	return 0;
}

int check_file_name(file_t* file, char const* name)
{
	if (file == NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	int cmp = strcmp(file->name, name);
	UNLOCK(file);

	if (cmp == 0) return 0;
	else return 1;
}

int is_open_file(file_t* file)
{
	if (file == NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	ssize_t osize = file->open_size;
	UNLOCK(file);

	if (osize == -1) return 1;
	else return 0;
}

int is_open_file_nolock(file_t* file)
{
	if (file == NULL) ERRSET(EINVAL, -1);
	if (file->open_size == -1) return 1;
	else return 0;
}

int is_locked_file(file_t* file, int who)
{
	if (file == NULL || who <= OWNER_NEXIST) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	int owner = file->owner;
	UNLOCK(file);

	if (owner == who) return 0;
	else return 1;
}

int get_size(file_t* file, size_t* data_size)
{
	if (file == NULL || data_size == NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	*data_size = file->data_size + file->new_size;
	UNLOCK(file);
	return 0;
}

int get_usage_data(file_t* file, time_t* fifo, int* lfu, char* lru)
{
	if (file == NULL || (fifo == NULL && lfu == NULL && lru == NULL)) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	if (fifo != NULL) *fifo = file->fifo_creation_time;
	if (lfu != NULL) *lfu = file->lfu_frequency;
	if (lru != NULL) *lru = file->lru_clock;
	UNLOCK(file);
	return 0;
}

int update_lru(file_t* file, char newval)
{
	if (file == NULL || newval > 1 || newval < 0) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	file->lru_clock = newval;
	UNLOCK(file);
	return 0;
}

int open_file(file_t* file, int who)
{
	if (file == NULL || who <= OWNER_NEXIST) ERRSET(EINVAL, -1);

	LOCK(file);
	CHECK_EXISTENCE(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EAGAIN, UNLOCK(file), -1);

	int open = is_open_file_nolock(file);
	if (open == -1) { UNLOCK(file); return -1; }
	else if(open == 1)
	{
		if (file->data_size == 0)
			file->open_size = 0;
		else
		{
			//decompress the file data into the open_data pointer.
			//compression not implemented yet.
			file->open_size = file->data_size;
			MALLOCDO(file->open_data, file->open_size, { UNLOCK(file); });
			memcpy(file->open_data, file->data, file->data_size);
		}
	}

	file->owner = who;
	file->lfu_frequency += 1;
	file->lru_clock = 1;
	UNLOCK(file);
	return 0;
}

int close_file(file_t* file, int who, long* difference)
{
	if (file == NULL || who <= OWNER_NEXIST) ERRSET(EINVAL, -1);

	LOCK(file);
	CHECK_EXISTENCE(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EAGAIN, UNLOCK(file), -1);
	int open = is_open_file_nolock(file);
	if (open == -1) { UNLOCK(file); return -1; }
	else if(open == 0)
	{
		//compress the file open_data into the data pointer.
		if (file->open_size > 0)
		{
			size_t old_size = file->data_size + file->new_size;

			//compression not implemented yet.
			file->data_size = file->open_size;
			REALLOCDO(file->data, file->data, file->data_size, { UNLOCK(file); });
			memcpy(file->data, file->open_data, file->data_size);

			*difference = (old_size - file->data_size);
		}

		free(file->open_data);
		file->open_data = NULL;
		file->open_size = -1;
		file->new_size = 0;
		file->owner = OWNER_NULL;
	}

	file->lfu_frequency += 1;
	file->lru_clock = 1;
	UNLOCK(file);
	return 0;
}

int read_file(file_t* file, int who, void** out_data, size_t* out_data_size, size_t* read_size)
{
	if(file == NULL || who <= OWNER_NEXIST ||
		out_data == NULL || out_data_size == NULL
		|| read_size == NULL) ERRSET(EINVAL, -1);

	LOCK(file);
	CHECK_EXISTENCE(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EAGAIN, UNLOCK(file), -1);
	CHECK_OPEN_FILE(file);
	if(*out_data_size < file->open_size)
	{
		void* out;
		REALLOCDO(out, *out_data, file->open_size, { UNLOCK(file); });
		*out_data = out;
		*out_data_size = file->open_size;
	}

	*read_size = file->open_size;
	memcpy(*out_data, file->open_data, file->open_size);
	file->lfu_frequency += 1;
	file->lru_clock = 1;
	UNLOCK(file);
	return 0;
}

int force_open_file(file_t* file)
{
	if (file == NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	file->owner = OWNER_ADMIN;
	UNLOCK(file);

	ERRCHECK(open_file(file, OWNER_ADMIN));
	return 0;
}

int force_read_file(file_t* file, void** out_data, size_t* out_data_size, size_t* read_size)
{
	if (file == NULL || out_data == NULL || out_data_size == NULL
		|| read_size == NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	file->owner = OWNER_ADMIN;
	UNLOCK(file);

	ERRCHECK(read_file(file, OWNER_ADMIN, out_data, out_data_size, read_size));
	return 0;
}
 
int write_file(file_t* file, int who, const void* data, size_t data_size)
{
	if(file == NULL || who <= OWNER_NEXIST ||
		data == NULL || data_size == 0) ERRSET(EINVAL, -1);

	LOCK(file);
	CHECK_EXISTENCE(file);
	if (file->owner != who)
		ERRSETDO(EAGAIN, UNLOCK(file), -1);
	CHECK_OPEN_FILE(file);
	if(file->open_size != 0) ERRSETDO(EEXIST, UNLOCK(file), -1);

	MALLOCDO(file->open_data, data_size, UNLOCK(file));
	file->open_size = data_size;
	memcpy(file->open_data, data, data_size);
	file->new_size = data_size;
	file->lfu_frequency += 1;
	file->lru_clock = 1;
	UNLOCK(file);
	return 0;
}

int append_file(file_t* file, int who, const void* data, size_t data_size)
{
	if(file == NULL || who <= OWNER_NEXIST ||
		data == NULL || data_size == 0) ERRSET(EINVAL, -1);

	LOCK(file);
	CHECK_EXISTENCE(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EAGAIN, UNLOCK(file), -1);
	CHECK_OPEN_FILE(file);
	if(file->open_size == 0) ERRSETDO(ENOENT, UNLOCK(file), -1);

	void* out;
	REALLOCDO(out, file->open_data, file->open_size + data_size, UNLOCK(file));
	file->open_data = out;

	memcpy((char*)file->open_data + file->open_size, data, data_size);
	file->open_size += data_size;
	file->new_size += data_size;
	file->lfu_frequency += 1;
	file->lru_clock = 1;
	UNLOCK(file);
	return 0;
}

int lock_file(file_t* file, int who)
{
	if(file == NULL || who <= OWNER_NEXIST) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	if (file->owner != OWNER_NULL && file->owner != who)
		ERRSETDO(EAGAIN, UNLOCK(file), -1);
	CHECK_OPEN_FILE(file);

	file->owner = who;
	file->lfu_frequency += 1;
	file->lru_clock = 1;
	UNLOCK(file);
	return 0;
}

int unlock_file(file_t* file, int who)
{
	if(file == NULL || who <= OWNER_NEXIST) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	if (file->owner != who)
		ERRSETDO(EAGAIN, UNLOCK(file), -1);
	CHECK_OPEN_FILE(file);

	file->owner = OWNER_NULL;
	file->lfu_frequency += 1;
	file->lru_clock = 1;
	UNLOCK(file);
	return 0;
}

int remove_file(file_t* file, int who, size_t* data_size)
{
	if(file == NULL || who < OWNER_NEXIST) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	if (file->owner == OWNER_NULL || file->owner != who)
		ERRSETDO(EAGAIN, UNLOCK(file), -1);
	*data_size = file->data_size + file->new_size;
	ERRCHECKDO(reset_file_struct(file), UNLOCK(file));
	UNLOCK(file);
	return 0;	
}

int force_remove_file(file_t* file, size_t* data_size)
{
	if (file == NULL) ERRSET(EINVAL, -1);
	LOCK(file);
	CHECK_EXISTENCE(file);
	file->owner = OWNER_ADMIN;
	UNLOCK(file);

	ERRCHECK(remove_file(file, OWNER_ADMIN, data_size));
	return 0;
}