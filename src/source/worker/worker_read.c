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

int do_read_file(int thread_id, int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state)
{
	char name[FILE_NAME_MAX_SIZE];
	ERRCHECK(read_file_name(in_msg, name, FILE_NAME_MAX_SIZE));
	SET_EMPTY_STRING(lastop_writefile_pname);

	out_msg->type = MESSAGE_RFILE_ACK;

	int fex = get_file(files, file_num, name);
	if (fex >= 0)
	{
		void* buf = NULL; size_t buf_size = 0, read_size;
		int lck = read_file(&files[fex], *conn, &buf, &buf_size, &read_size);
		if (lck == 0)
		{
			out_msg->type |= MESSAGE_OP_SUCC;

			if(read_size != 0)
				ERRCHECK(push_buf(&out_msg->data, sizeof(char) * read_size, buf));
			ERRCHECK(push_buf(&out_msg->data, sizeof(size_t), &read_size));
			free(buf);

			do_log(log, thread_id, *conn, name, STRING_READ_FILE, "Success.", (int)read_size, 0);
		}
		else if (errno == ENOENT)
		{
			out_msg->type |= MESSAGE_FILE_NEXISTS;
			do_log(log, thread_id, *conn, name, STRING_READ_FILE, "File doesn't exist.", 0, 0);
		}
		else if (errno == EAGAIN)
		{
			out_msg->type |= MESSAGE_FILE_NOWN;
			do_log(log, thread_id, *conn, name, STRING_READ_FILE, "Permission denied.", 0, 0);
		}
		else if (errno == EPERM)
		{
			out_msg->type |= MESSAGE_FILE_NOPEN;
			do_log(log, thread_id, *conn, name, STRING_READ_FILE, "File is closed.", 0, 0);
		}
		else
		{
			do_log(log, thread_id, *conn, name, STRING_READ_FILE, "File error.", 0, 0);
			return -1;
		}
	}
	else if (errno == ENOENT)
	{
		out_msg->type |= MESSAGE_FILE_NEXISTS;

		do_log(log, thread_id, *conn, name, STRING_READ_FILE, "File doesn't exist.", 0, 0);
	}
	else
	{
		do_log(log, thread_id, *conn, name, STRING_READ_FILE, "File error.", 0, 0);
		return -1;
	}
	return 0;
}

int do_readn_files(int thread_id, int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state)
{
	SET_EMPTY_STRING(lastop_writefile_pname);

	out_msg->type = MESSAGE_RNFILE_ACK;
	int n, res, error; size_t count = 0;
	ERRCHECK(pop_buf(&in_msg->data, sizeof(int), &n));

	long diff; int wasclosed;
	void* buf = NULL; void* name = NULL; size_t buf_size = 0, name_size = 0;
	size_t read_buf, read_name;
	int total_read = 0;
	for (size_t i = 0; i < file_num && (n <= 0 || count < n); i++)
	{
		wasclosed = is_open_file(&files[i]);
		if (open_file(&files[i], *conn) == 0)
		{
			res = read_file(&files[i], *conn, &buf, &buf_size, &read_buf);
			error = errno;

			if (wasclosed == 1)
			{
				diff = 0;
				if (close_file(&files[i], *conn, &diff) == -1 && errno != ENOENT && errno != EAGAIN) { free(buf); free(name); return -1; }
				if (diff != 0)
				{
					ERRCHECK(pthread_mutex_lock(&state->state_mux));
					state->current_storage -= diff;
					ERRCHECK(pthread_mutex_unlock(&state->state_mux));
				}
			}

			if (res == 0)
			{
				if (get_file_name(&files[i], (char**)(&name), &name_size, &read_name) == 0)
				{
					if(read_buf != 0)
						ERRCHECKDO(push_buf(&out_msg->data, sizeof(char) * read_buf, buf), { free(buf); free(name); });
					ERRCHECKDO(push_buf(&out_msg->data, sizeof(size_t), &read_buf), { free(buf); free(name); });
					total_read += read_buf;

					ERRCHECKDO(convert_slashes_to_underscores((char*)name), { free(buf); free(name); });
					ERRCHECKDO(push_buf(&out_msg->data, sizeof(char) * read_name, name), { free(buf); free(name); });
					ERRCHECKDO(push_buf(&out_msg->data, sizeof(size_t), &read_name), { free(buf); free(name); });
					count++;
				}
				else if (error != ENOENT)
				{
					free(name);
					free(buf);
					do_log(log, thread_id, *conn, "none", STRING_READN_FILE, "File error.", 0, 0);
					return -1;
				}
			}
			else if (error != EAGAIN && error != EPERM && error != ENOENT)
			{
				do_log(log, thread_id, *conn, "none", STRING_READN_FILE, "File error.", 0, 0);
				free(name);
				free(buf);
				return -1;
			}
		}
		else if (errno != ENOENT && errno != EAGAIN) { free(buf); free(name); return -1; }
	}

	free(buf);
	free(name);
	ERRCHECK(push_buf(&out_msg->data, sizeof(size_t), &count));
	out_msg->type |= MESSAGE_OP_SUCC;

	do_log(log, thread_id, *conn, "none", STRING_READN_FILE, "Success.", total_read, 0);
	return 0;
}