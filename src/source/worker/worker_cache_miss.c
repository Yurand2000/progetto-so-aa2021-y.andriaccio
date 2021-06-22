#include "worker_generics.h"

#include <errno.h>

#include "../file.h"
#include "../net_msg.h"
#include "../errset.h"

int cache_miss(file_t* files, size_t file_num, shared_state* state, net_msg* out_msg)
{
	void* buf = NULL; void* name = NULL; size_t buf_size = 0, name_size = 0;
	//cache miss call here
	switch (state->ro_cache_miss_algorithm)
	{
	case ALGO_LFU:
		evict_LFU(files, file_num, state, &buf, &buf_size, &name, &name_size);
		break;
	case ALGO_LRU:
		evict_LRU(files, file_num, state, &buf, &buf_size, &name, &name_size);
		break;
	case ALGO_FIFO:
		evict_FIFO(files, file_num, state, &buf, &buf_size, &name, &name_size);
	default:
		break;
	}	
	ERRCHECKDO(push_buf(&out_msg->data, sizeof(char) * buf_size, buf), { free(buf); free(name); });
	ERRCHECKDO(push_buf(&out_msg->data, sizeof(size_t), &buf_size), { free(buf); free(name); });
	ERRCHECKDO(push_buf(&out_msg->data, sizeof(char) * name_size, name), { free(buf); free(name); });
	ERRCHECKDO(push_buf(&out_msg->data, sizeof(size_t), &name_size), { free(buf); free(name); });

	ERRCHECK(pthread_mutex_lock(&state->state_mux));
	state->cache_miss_execs++;
	ERRCHECK(pthread_mutex_unlock(&state->state_mux));

	free(buf);
	free(name);
	return 0;
}

int delete_evicted(int file, file_t* files, shared_state* state,
	void** buf, size_t* buf_size, void** name, size_t* name_size)
{
	size_t storage;
	ERRCHECK(get_size(&files[file], &storage));

	ERRCHECK(force_read_file(&files[file], buf, buf_size, buf_size));
	ERRCHECK(get_file_name(&files[file], name, name_size, name_size));
	ERRCHECK(force_remove_file(&files[file]));

	ERRCHECK(pthread_mutex_lock(&state->state_mux));
	state->current_storage -= storage;
	state->current_files--;
	ERRCHECK(pthread_mutex_unlock(&state->state_mux));
	return 0;
}

int evict_FIFO(file_t* files, size_t file_num, shared_state* state,
	void** buf, size_t* buf_size, void** name, size_t name_size)
{
	size_t curr_older = 0;
	time_t curr_older_time = time(NULL);
	time_t temp;
	for (size_t i = 0; i < file_num; i++)
	{
		get_usage_data(&files[i], &temp, NULL, NULL);
		if (temp < curr_older_time)
		{
			curr_older_time = temp;
			curr_older = i;
		}
	}

	ERRCHECK(delete_evicted(curr_older, files, state, buf, buf_size, name, name_size));
	return 0;
}

int evict_LRU(file_t* files, size_t file_num, shared_state* state,
	void** buf, size_t* buf_size, void** name, size_t name_size)
{
	ERRCHECK(pthread_mutex_lock(&state->state_mux));
	size_t clock_pos = state->last_evicted;
	ERRCHECK(pthread_mutex_unlock(&state->state_mux));
	char temp = 1;

	for (size_t i = 0; i < file_num && temp; i++)
	{
		get_usage_data(&files[clock_pos], NULL, NULL, &temp);
		if (temp)
		{
			update_lru(&files[clock_pos], 0);
			clock_pos = (clock_pos + 1) % file_num;
		}
	}

	ERRCHECK(delete_evicted(clock_pos, files, state, buf, buf_size, name, name_size));
	return 0;
}

int evict_LFU(file_t* files, size_t file_num, shared_state* state,
	void** buf, size_t* buf_size, void** name, size_t name_size)
{
	size_t curr_least_used = 0;
	int curr_least_used_freq = INT_MAX;
	int temp;
	for (size_t i = 0; i < file_num; i++)
	{
		get_usage_data(&files[i], NULL, &temp, NULL);
		if (temp < curr_least_used_freq)
		{
			curr_least_used_freq = temp;
			curr_least_used = i;
		}
	}

	ERRCHECK(delete_evicted(curr_least_used, files, state, buf, buf_size, name, name_size));
	return 0;
}