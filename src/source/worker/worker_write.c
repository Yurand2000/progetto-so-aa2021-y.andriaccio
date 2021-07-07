#include "../worker.h"
#include "worker_generics.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#include "../errset.h"
#include "../log.h"
#include "../net_msg.h"
#include "../message_type.h"
#include "../net_msg_macros.h"

static int do_cache_miss(log_t* log, int thread_id, char* name, file_t* files, size_t file_num, size_t buf_size,
	shared_state* state, net_msg* out_msg);
static int reserve_storage(size_t buf_size, shared_state* state);
static int get_data(int thread_id, int conn, char* name, net_msg* in_msg, void** buf,
	size_t* buf_size, shared_state* state, log_t* log, char* log_op);

int do_write_file(int thread_id, int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state)
{
	char name[FILE_NAME_MAX_SIZE];
	ERRCHECK(read_file_name(in_msg, name, FILE_NAME_MAX_SIZE));

	out_msg->type = MESSAGE_WFILE_ACK;
	if (strncmp(name, lastop_writefile_pname, FILE_NAME_MAX_SIZE) == 0)
	{
		SET_EMPTY_STRING(lastop_writefile_pname);

		int file = get_file(files, file_num, name);
		if (file == -1 && errno != ENOENT)
		{
			//file error!
			do_log(log, thread_id, *conn, name, STRING_WRITE_FILE, "File error.", 0, 0);
			return -1;
		}
		else if (file == -1)
		{
			out_msg->type |= MESSAGE_FILE_NEXISTS;
			do_log(log, thread_id, *conn, name, STRING_WRITE_FILE, "File doesn't exist.", 0, 0);
		}
		else
		{
			void* buf = NULL; size_t buf_size = 0; int ret;
			ERRCHECK( (ret = get_data(thread_id, *conn, name, in_msg, &buf, &buf_size, state, log, STRING_WRITE_FILE)) );
			if (ret == 1) return 0;

			ERRCHECKDO(do_cache_miss(log, thread_id, name, files, file_num, buf_size, state, out_msg), { free(buf); });
			ERRCHECKDO(reserve_storage(buf_size, state), { free(buf); });

			//write file
			ret = write_file(&files[file], *conn, buf, buf_size);
			free(buf);
			if (ret == -1)
			{
				//release storage
				ERRCHECK(pthread_mutex_lock(&state->state_mux));
				state->current_storage -= buf_size;
				ERRCHECK(pthread_mutex_unlock(&state->state_mux));

				if (errno == EPERM)
				{
					out_msg->type |= MESSAGE_FILE_NOPEN;
					do_log(log, thread_id, *conn, name, STRING_WRITE_FILE, "File is closed.", 0, 0);
				}
				else if (errno == ENOENT)
				{
					out_msg->type |= MESSAGE_FILE_NEXISTS;
					do_log(log, thread_id, *conn, name, STRING_WRITE_FILE, "File doesn't exist.", 0, 0);
				}
				else if(errno == EEXIST)
				{
					out_msg->type |= MESSAGE_FILE_EXISTS;
					do_log(log, thread_id, *conn, name, STRING_WRITE_FILE, "File was not empty.", 0, 0);
				}
				else if (errno == EAGAIN)
				{
					out_msg->type |= MESSAGE_FILE_NLOCK;
					do_log(log, thread_id, *conn, name, STRING_WRITE_FILE, "Permission denied.", 0, 0);
				}
				else
				{
					//file error!
					do_log(log, thread_id, *conn, name, STRING_WRITE_FILE, "File error.", 0, 0);
					return -1;
				}
			}
			else
			{
				out_msg->type |= MESSAGE_OP_SUCC;
				do_log(log, thread_id, *conn, name, STRING_WRITE_FILE, "Success.", 0, buf_size);
			}
		}
	}
	else
	{
		do_log(log, thread_id, *conn, name, STRING_WRITE_FILE, "Client last operation was not a"
			" successful open file request with O_CREATE and O_LOCK flags.", 0, 0);
	}
	return 0;
}

int do_append_file(int thread_id, int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state)
{
	char name[FILE_NAME_MAX_SIZE];
	ERRCHECK(read_file_name(in_msg, name, FILE_NAME_MAX_SIZE));
	SET_EMPTY_STRING(lastop_writefile_pname);

	out_msg->type = MESSAGE_AFILE_ACK;

	int file = get_file(files, file_num, name);
	if (file == -1 && errno != ENOENT)
	{
		//file error!
		do_log(log, thread_id, *conn, name, STRING_APPEND_FILE, "File error.", 0, 0);
		return -1;
	}
	else if (file == -1)
	{
		out_msg->type |= MESSAGE_FILE_NEXISTS;

		do_log(log, thread_id, *conn, name, STRING_APPEND_FILE, "File doesn't exist.", 0, 0);
	}
	else
	{
		void* buf = NULL; size_t buf_size = 0; int ret;
		ERRCHECK((ret = get_data(thread_id, *conn, name, in_msg, &buf, &buf_size, state, log, STRING_WRITE_FILE)));
		if (ret == 1) { free(buf); return 0; }

		ERRCHECKDO(do_cache_miss(log, thread_id, name, files, file_num, buf_size, state, out_msg), { free(buf); });
		ERRCHECKDO(reserve_storage(buf_size, state), { free(buf); });

		//append file
		ret = append_file(&files[file], *conn, buf, buf_size);
		free(buf);
		if (ret == -1)
		{
			//release storage
			ERRCHECK(pthread_mutex_lock(&state->state_mux));
			state->current_storage -= buf_size;
			ERRCHECK(pthread_mutex_unlock(&state->state_mux));

			if (errno == EPERM)
			{
				out_msg->type |= MESSAGE_FILE_NOPEN;
				do_log(log, thread_id, *conn, name, STRING_APPEND_FILE, "File is closed.", 0, 0);
			}
			else if (errno == ENOENT)
			{
				out_msg->type |= MESSAGE_FILE_NEXISTS;
				do_log(log, thread_id, *conn, name, STRING_APPEND_FILE, "File was empty or didn't exist anymore.", 0, 0);
			}
			else if (errno == EAGAIN)
			{
				out_msg->type |= MESSAGE_FILE_NLOCK;
				do_log(log, thread_id, *conn, name, STRING_APPEND_FILE, "Permission denied.", 0, 0);
			}
			else
			{
				//file error!
				do_log(log, thread_id, *conn, name, STRING_APPEND_FILE, "File error.", 0, 0);
				free(buf);
				return -1;
			}
		}
		else
		{
			out_msg->type |= MESSAGE_OP_SUCC;
			do_log(log, thread_id, *conn, name, STRING_APPEND_FILE, "Success.", 0, buf_size);
		}
	}
	return 0;
}

static int do_cache_miss(log_t* log, int thread_id, char* name, file_t* files, size_t file_num, size_t buf_size,
	shared_state* state, net_msg* out_msg)
{
	size_t curr_storage; size_t count = 0;
	ERRCHECK(pthread_mutex_lock(&state->state_mux));
	curr_storage = state->current_storage;
	ERRCHECK(pthread_mutex_unlock(&state->state_mux));

	while (curr_storage + buf_size > state->ro_max_storage)
	{
		int err = cache_miss(log, thread_id, name, files, file_num, state, out_msg);
		if (err == 0)
			count++;
		else if (err == -1 && errno != ECANCELED) return -1;

		ERRCHECK(pthread_mutex_lock(&state->state_mux));
		curr_storage = state->current_storage;
		ERRCHECK(pthread_mutex_unlock(&state->state_mux));
	}

	if (count > 0)
	{
		out_msg->type |= MESSAGE_FILE_CHACEMISS;
		push_buf(&out_msg->data, sizeof(size_t), &count);
	}
	return 0;
}

static int reserve_storage(size_t buf_size, shared_state* state)
{
	ERRCHECK(pthread_mutex_lock(&state->state_mux));
	state->current_storage += buf_size;
	if (state->current_storage >= state->max_reached_storage)
		state->max_reached_storage = state->current_storage;
	ERRCHECK(pthread_mutex_unlock(&state->state_mux));

	return 0;
}

static int get_data(int thread_id, int conn, char* name, net_msg* in_msg, void** buf,
	size_t* buf_size, shared_state* state, log_t* log, char* log_op)
{
	ERRCHECK(pop_buf(&in_msg->data, sizeof(size_t), buf_size));

	if (*buf_size > state->ro_max_storage)
	{
		do_log(log, thread_id, conn, name, log_op, "Data is too big.", 0, 0);
		return 1;
	}

	MALLOC(*buf, sizeof(char) * *buf_size);
	ERRCHECKDO(pop_buf(&in_msg->data, sizeof(char) * *buf_size, *buf), { free(*buf); });

	return 0;
}