#include "worker_generics.h"

#include <limits.h>
#include <errno.h>

#include "../file.h"
#include "../net_msg.h"
#include "../errset.h"
#include "../message_type.h"

#define FILE_LOOP_DO(nodel_file, files, file_num, todo) \
int res;\
for (size_t i = 0; i < file_num; i++)\
{\
	ERRCHECK( (res = is_existing_file(&files[i])) );\
	if (res == 0)\
	{\
		ERRCHECK( (res = check_file_name(&files[i], nodel_file)) );\
		if (res != 0)\
		{\
			if((files[i].data_size + files[i].open_size) > 0)\
				todo;\
		}\
	}\
}\

int cache_miss(log_t* log, int thread, char* nodel_file, file_t* files, size_t file_num, shared_state* state, net_msg* out_msg)
{
	void* buf = NULL; char* name = NULL; size_t buf_size = 0, name_size = 0;
	//cache miss call here
	switch (state->ro_cache_miss_algorithm)
	{
	case ALGO_LFU:
		ERRCHECKDO(evict_LFU(log, thread, nodel_file, files, file_num, state,
			&buf, &buf_size, &name, &name_size), { free(buf); free(name); });
		break;
	case ALGO_LRU:
		ERRCHECKDO(evict_LRU(log, thread, nodel_file, files, file_num, state,
			&buf, &buf_size, &name, &name_size), { free(buf); free(name); });
		break;
	case ALGO_FIFO:
		ERRCHECKDO(evict_FIFO(log, thread, nodel_file, files, file_num, state,
			&buf, &buf_size, &name, &name_size), { free(buf); free(name); });
	default:
		break;
	}

	PTRCHECKDO(buf, { free(name); });
	PTRCHECKDO(name, { free(buf); });
	ERRCHECKDO(convert_slashes_to_underscores(name), { free(buf); free(name); });
	ERRCHECKDO(push_buf(&out_msg->data, sizeof(char) * buf_size, buf), { free(buf); free(name); });
	ERRCHECKDO(push_buf(&out_msg->data, sizeof(size_t), &buf_size), { free(buf); free(name); });
	ERRCHECKDO(push_buf(&out_msg->data, sizeof(char) * name_size, name), { free(buf); free(name); });
	ERRCHECKDO(push_buf(&out_msg->data, sizeof(size_t), &name_size), { free(buf); free(name); });

	free(buf);
	free(name);

	ERRCHECK(pthread_mutex_lock(&state->state_mux));
	state->cache_miss_execs++;
	ERRCHECK(pthread_mutex_unlock(&state->state_mux));
	return 0;
}

int delete_evicted(log_t* log, int thread, int file, file_t* files, shared_state* state,
	void** buf, size_t* buf_size, char** name, size_t* name_size)
{
	size_t storage;
	ERRCHECK(get_size(&files[file], &storage));

	ERRCHECK(force_open_file(&files[file]));
	ERRCHECK(force_read_file(&files[file], buf, buf_size, buf_size));
	ERRCHECK(get_file_name(&files[file], name, name_size, name_size));
	ERRCHECK(force_remove_file(&files[file]));

	ERRCHECK(pthread_mutex_lock(&state->state_mux));
	state->current_storage -= storage;
	state->current_files--;
	ERRCHECK(pthread_mutex_unlock(&state->state_mux));

	do_log(log, thread, 0, *name, STRING_CACHE_MISS, "Success.", 0, 0);
	return 0;
}

int evict_FIFO(log_t* log, int thread, char* nodel_file, file_t* files, size_t file_num, shared_state* state,
	void** buf, size_t* buf_size, char** name, size_t* name_size)
{
	size_t curr_older = 0;
	time_t curr_older_time = time(NULL);
	time_t temp; int selected = 0;
	
	FILE_LOOP_DO(nodel_file, files, file_num, 
	{
		get_usage_data(&files[i], &temp, NULL, NULL);
		if (temp < curr_older_time)
		{
			curr_older_time = temp;
			curr_older = i;
			selected = 1;
		}
	});

	if(selected) ERRCHECK(delete_evicted(log, thread, curr_older, files, state, buf, buf_size, name, name_size));
	return 0;
}

int evict_LRU(log_t* log, int thread, char* nodel_file, file_t* files, size_t file_num, shared_state* state,
	void** buf, size_t* buf_size, char** name, size_t* name_size)
{
	ERRCHECK(pthread_mutex_lock(&state->state_mux));
	size_t clock_pos = state->last_evicted;
	ERRCHECK(pthread_mutex_unlock(&state->state_mux));
	char temp = 1, selected = 0;

	FILE_LOOP_DO(nodel_file, files, file_num,
	{
		get_usage_data(&files[clock_pos], NULL, NULL, &temp);
		if (temp)
		{
			update_lru(&files[clock_pos], 0);
			clock_pos = (clock_pos + 1) % file_num;
			selected = 1;
		}
	});

	if (selected) ERRCHECK(delete_evicted(log, thread, clock_pos, files, state, buf, buf_size, name, name_size));
	return 0;
}

int evict_LFU(log_t* log, int thread, char* nodel_file, file_t* files, size_t file_num, shared_state* state,
	void** buf, size_t* buf_size, char** name, size_t* name_size)
{
	size_t curr_least_used = 0;
	int curr_least_used_freq = INT_MAX;
	int temp, selected = 0;
	
	FILE_LOOP_DO(nodel_file, files, file_num,
	{
		get_usage_data(&files[i], NULL, &temp, NULL);
		if (temp < curr_least_used_freq)
		{
			curr_least_used_freq = temp;
			curr_least_used = i;
			selected = 1;
		}
	});

	if(selected) ERRCHECK(delete_evicted(log, thread, curr_least_used, files, state, buf, buf_size, name, name_size));
	return 0;
}