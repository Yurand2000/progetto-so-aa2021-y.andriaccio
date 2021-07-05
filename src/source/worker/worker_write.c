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

static int do_cache_miss(char* name, file_t* files, size_t file_num, size_t buf_size,
	shared_state* state, net_msg* out_msg);
static int reserve_storage(size_t buf_size, shared_state* state);
static int get_data(int conn, char* name, net_msg* in_msg, void** buf,
	size_t* buf_size, shared_state* state, log_t* log, char* log_op);

int do_write_file(int* conn, net_msg* in_msg, net_msg* out_msg,
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
			do_log(log, *conn, STRING_WRITE_FILE, name, "File error.");
			return -1;
		}
		else if (file == -1)
		{
			out_msg->type |= MESSAGE_FILE_NEXISTS;

			do_log(log, *conn, STRING_WRITE_FILE, name, "File doesn't exist.");
		}
		else
		{
			void* buf = NULL; size_t buf_size = 0; int ret;
			ERRCHECK( (ret = get_data(*conn, name, in_msg, &buf, &buf_size, state, log, STRING_WRITE_FILE)) );
			if (ret == 1) { free(buf); return 0; }

			ERRCHECKDO(do_cache_miss(name, files, file_num, buf_size, state, out_msg), { free(buf); });
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
					do_log(log, *conn, STRING_WRITE_FILE, name, "File is closed.");
				}
				else if(errno == EEXIST)
				{
					out_msg->type |= MESSAGE_FILE_EXISTS;
					do_log(log, *conn, STRING_WRITE_FILE, name, "File was not empty.");
				}
				else if (errno == EAGAIN)
				{
					out_msg->type |= MESSAGE_FILE_NLOCK;
					do_log(log, *conn, STRING_WRITE_FILE, name, "Permission denied.");
				}
				else
				{
					//file error!
					do_log(log, *conn, STRING_WRITE_FILE, name, "File error.");
					return -1;
				}
			}
			else
			{
				out_msg->type |= MESSAGE_OP_SUCC;
				do_log(log, *conn, STRING_WRITE_FILE, name, "Success.");
			}
		}
	}
	else
	{
		do_log(log, *conn, STRING_WRITE_FILE, name, "Client last operation was not a"
			" successful open file request with O_CREATE and O_LOCK flags.");
	}
	return 0;
}

int do_append_file(int* conn, net_msg* in_msg, net_msg* out_msg,
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
		do_log(log, *conn, STRING_APPEND_FILE, name, "File error.");
		return -1;
	}
	else if (file == -1)
	{
		out_msg->type |= MESSAGE_FILE_NEXISTS;

		do_log(log, *conn, STRING_APPEND_FILE, name, "File doesn't exist.");
	}
	else
	{
		void* buf = NULL; size_t buf_size = 0; int ret;
		ERRCHECK((ret = get_data(*conn, name, in_msg, &buf, &buf_size, state, log, STRING_WRITE_FILE)));
		if (ret == 1) { free(buf); return 0; }

		ERRCHECKDO(do_cache_miss(name, files, file_num, buf_size, state, out_msg), { free(buf); });
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
				do_log(log, *conn, STRING_APPEND_FILE, name, "File is closed.");
			}
			else if (errno == ENOENT)
			{
				out_msg->type |= MESSAGE_FILE_NEXISTS;
				do_log(log, *conn, STRING_APPEND_FILE, name, "File was empty.");
			}
			else if (errno == EAGAIN)
			{
				out_msg->type |= MESSAGE_FILE_NLOCK;
				do_log(log, *conn, STRING_APPEND_FILE, name, "Permission denied.");
			}
			else
			{
				//file error!
				do_log(log, *conn, STRING_APPEND_FILE, name, "File error.");
				free(buf);
				return -1;
			}
		}
		else
		{
			out_msg->type |= MESSAGE_OP_SUCC;
			do_log(log, *conn, STRING_APPEND_FILE, name, "Success.");
		}
	}
	return 0;
}

static int do_cache_miss(char* name, file_t* files, size_t file_num, size_t buf_size,
	shared_state* state, net_msg* out_msg)
{
	size_t curr_storage; size_t count = 0;
	ERRCHECK(pthread_mutex_lock(&state->state_mux));
	curr_storage = state->current_storage;
	ERRCHECK(pthread_mutex_unlock(&state->state_mux));

	while (curr_storage + buf_size >= state->ro_max_storage)
	{
		cache_miss(name, files, file_num, state, out_msg);

		ERRCHECK(pthread_mutex_lock(&state->state_mux));
		curr_storage = state->current_storage;
		ERRCHECK(pthread_mutex_unlock(&state->state_mux));
		count++;
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

static int get_data(int conn, char* name, net_msg* in_msg, void** buf,
	size_t* buf_size, shared_state* state, log_t* log, char* log_op)
{
	ERRCHECK(pop_buf(&in_msg->data, sizeof(size_t), buf_size));

	if (*buf_size > state->ro_max_storage)
	{
		do_log(log, conn, log_op, name, "Data is too big.");
		return 1;
	}

	MALLOC(*buf, sizeof(char) * *buf_size);
	ERRCHECKDO(pop_buf(&in_msg->data, sizeof(char) * *buf_size, *buf), { free(*buf); });

	return 0;
}