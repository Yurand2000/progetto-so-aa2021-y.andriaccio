#include "worker_generics.h"

#include <limits.h>
#include <errno.h>

#include "../file.h"
#include "../net_msg.h"
#include "../errset.h"
#include "../message_type.h"

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
	default:
	case ALGO_FIFO:
		ERRCHECKDO(evict_FIFO(log, thread, nodel_file, files, file_num, state,
			&buf, &buf_size, &name, &name_size), { free(buf); free(name); });
		break;
	}

	if (name != NULL && buf != NULL)
	{
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
	else
	{
		do_log(log, thread, 0, "none", STRING_CACHE_MISS, "Skipped.", 0, 0);
		ERRSET(ECANCELED, -1);
	}
}

int delete_evicted(log_t* log, int thread, int file, file_t* files, shared_state* state,
	void** buf, size_t* buf_size, char** name, size_t* name_size)
{
	long storage = 0; int ret;
	ret = force_open_file(&files[file]);
	if (ret == 0)
	{
		ERRCHECK(force_read_file(&files[file], buf, buf_size, buf_size));

		ERRCHECK(get_file_name(&files[file], name, name_size, name_size));
		ERRCHECK(force_remove_file(&files[file], &storage));

		do_log(log, thread, 0, *name, STRING_CACHE_MISS, "Success.", 0, 0);

		ERRCHECK(pthread_mutex_lock(&state->state_mux));
		state->current_storage -= storage;
		state->current_files--;
		ERRCHECK(pthread_mutex_unlock(&state->state_mux));
	}
	else if (errno != ENOENT && errno != EAGAIN) return -1;

	return 0;
}

static int loop_check(size_t i, char* nodel_file, file_t* files, size_t file_num)
{
	int ret; size_t datasize;
	ret = is_locked_file(&files[i], OWNER_ADMIN);
	if (ret == 0) return 1;
	else if (ret == -1 && errno != ENOENT) return -1;

	ret = check_file_name(&files[i], nodel_file);
	if (ret == 0) return 1;
	else if (ret == -1 && errno != ENOENT) return -1;

	ret = get_size(&files[i], &datasize);
	if (ret == -1)
	{
		if (errno == ENOENT) return 1;
		else if (errno != ENOENT) return -1;
	}

	if (datasize > 0) return 0;
	else return 1;
}

static int FIFO_loop_do(size_t i, char* nodel_file, file_t* files, size_t file_num, size_t* curr_older, time_t* curr_older_time, int* selected)
{
	int ret; time_t temp;
	ERRCHECK( (ret = loop_check(i, nodel_file, files, file_num)) );
	if (ret == 1) return 0;

	ret = get_usage_data(&files[i], &temp, NULL, NULL);
	if (ret == 0)
	{
		if (temp < *curr_older_time)
		{
			*curr_older_time = temp;
			*curr_older = i;
			*selected = 1;
		}
	}
	else if (errno != ENOENT) return -1;

	return 0;
}

int evict_FIFO(log_t* log, int thread, char* nodel_file, file_t* files, size_t file_num, shared_state* state,
	void** buf, size_t* buf_size, char** name, size_t* name_size)
{
	size_t curr_older = 0;
	time_t curr_older_time = time(NULL);
	int selected = 0;

	for (size_t i = 0; i < file_num; i++)
	{
		ERRCHECK( (FIFO_loop_do(i, nodel_file, files,
				   file_num, &curr_older, &curr_older_time, &selected)) );
	}
		

	if (selected == 1)
		ERRCHECK(delete_evicted(log, thread, curr_older, files, state, buf, buf_size, name, name_size));
	return 0;
}

static int LRU_loop_do(size_t i, char* nodel_file, file_t* files, size_t file_num)
{
	int ret; char temp;
	ERRCHECK( (ret = loop_check(i, nodel_file, files, file_num)) );
	if (ret == 1) return 0;

	ret = get_usage_data(&files[i], NULL, NULL, &temp);
	if (ret == 0)
	{
		if (temp == 1)
			update_lru(&files[i], 0);
		else
			return 1;
	}
	else if (errno != ENOENT) return -1;

	return 0;
}

int evict_LRU(log_t* log, int thread, char* nodel_file, file_t* files, size_t file_num, shared_state* state,
	void** buf, size_t* buf_size, char** name, size_t* name_size)
{
	ERRCHECK(pthread_mutex_lock(&state->state_mux));
	size_t clock_pos = state->lru_clock_pos;
	ERRCHECK(pthread_mutex_unlock(&state->state_mux));

	int exit = 0; size_t expel = 0;
	for (size_t i = 0; i < file_num && exit == 0; i++)
	{
		ERRCHECK( (exit = LRU_loop_do(clock_pos, nodel_file, files, file_num)) );
		if (exit == 1) expel = clock_pos;

		clock_pos = (clock_pos + 1) % file_num;
	}

	if (exit == 1)
	{
		ERRCHECK(delete_evicted(log, thread, expel, files, state, buf, buf_size, name, name_size));

		ERRCHECK(pthread_mutex_lock(&state->state_mux));
		state->lru_clock_pos = (expel + 1) % file_num;
		ERRCHECK(pthread_mutex_unlock(&state->state_mux));
	}	
	return 0;
}

static int LFU_loop_do(size_t i, char* nodel_file, file_t* files, size_t file_num, size_t* curr_least_used, int* curr_least_used_freq, int* selected)
{
	int ret; int temp;
	ERRCHECK((ret = loop_check(i, nodel_file, files, file_num)));
	if (ret == 1) return 0;
	
	ret = get_usage_data(&files[i], NULL, &temp, NULL);
	if (ret == 0)
	{
		if (temp < *curr_least_used_freq)
		{
			*curr_least_used_freq = temp;
			*curr_least_used = i;
			*selected = 1;
		}
	}
	else if (errno != ENOENT) return -1;

	return 0;
}

int evict_LFU(log_t* log, int thread, char* nodel_file, file_t* files, size_t file_num, shared_state* state,
	void** buf, size_t* buf_size, char** name, size_t* name_size)
{
	size_t curr_least_used = 0;
	int curr_least_used_freq = INT_MAX;
	int selected = 0;
	
	for (size_t i = 0; i < file_num; i++)
	{
		ERRCHECK( LFU_loop_do(i, nodel_file, files, file_num,
			      &curr_least_used, &curr_least_used_freq, &selected) );
	}

	if (selected == 1)
		ERRCHECK(delete_evicted(log, thread, curr_least_used, files, state, buf, buf_size, name, name_size));
	return 0;
}