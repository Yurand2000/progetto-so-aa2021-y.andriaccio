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

int do_read_file(int* conn, net_msg* in_msg, net_msg* out_msg,
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

			ERRCHECK(push_buf(&out_msg->data, sizeof(char) * read_size, buf));
			ERRCHECK(push_buf(&out_msg->data, sizeof(size_t), &read_size));
			free(buf);

			do_log(log, *conn, STRING_READ_FILE, name, "Success.");
		}
		else if (errno == EAGAIN)
		{
			out_msg->type |= MESSAGE_FILE_NOWN;
			do_log(log, *conn, STRING_READ_FILE, name, "Permission denied.");
		}
		else if (errno == EPERM)
		{
			out_msg->type |= MESSAGE_FILE_NOPEN;
			do_log(log, *conn, STRING_READ_FILE, name, "File is closed.");
		}
		else
		{
			do_log(log, *conn, STRING_READ_FILE, name, "File error.");
			return -1;
		}
	}
	else if (errno == ENOENT)
	{
		out_msg->type |= MESSAGE_FILE_NEXISTS;

		do_log(log, *conn, STRING_READ_FILE, name, "File doesn't exist.");
	}
	else
	{
		do_log(log, *conn, STRING_READ_FILE, name, "File error.");
		return -1;
	}
	return 0;
}

int do_readn_files(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state)
{
	SET_EMPTY_STRING(lastop_writefile_pname);

	out_msg->type = MESSAGE_RNFILE_ACK;
	int n, res, error; size_t count = 0;
	ERRCHECK(pop_buf(&in_msg->data, sizeof(int), &n));

	size_t i = 0; long diff;
	void* buf = NULL; size_t buf_size = 0, read_size;
	while (i < file_num && (n <= 0 || count < n))
	{
		if ((is_existing_file(&files[i]) == 0))
		{
			ERRCHECKDO(open_file(&files[i], *conn), { free(buf); });

			error = 0;
			res = read_file(&files[i], *conn, &buf, &buf_size, &read_size);
			error = errno;

			ERRCHECKDO(close_file(&files[i], *conn, &diff), { free(buf); });
			ERRCHECK(pthread_mutex_lock(&state->state_mux));
			state->current_storage -= diff;
			ERRCHECK(pthread_mutex_unlock(&state->state_mux));

			if (res == 0)
			{
				ERRCHECKDO(push_buf(&out_msg->data, sizeof(char) * read_size, buf), { free(buf); });
				ERRCHECKDO(push_buf(&out_msg->data, sizeof(size_t), &read_size), { free(buf); });

				if (get_file_name(&files[i], (char**)(&buf), &buf_size, &read_size) == -1)
				{
					do_log(log, *conn, STRING_READN_FILE, "none", "File error.");
					free(buf);
					return -1;
				}
				ERRCHECKDO(convert_slashes_to_underscores((char*)buf), { free(buf); });
				ERRCHECKDO(push_buf(&out_msg->data, sizeof(char) * read_size, buf), { free(buf); });
				ERRCHECKDO(push_buf(&out_msg->data, sizeof(size_t), &read_size), { free(buf); });
				count++;
			}
			else if(error != EAGAIN && error != EPERM)
			{
				do_log(log, *conn, STRING_READN_FILE, "none", "File error.");
				free(buf);
				return -1;
			}
		}
		i++;
	}

	free(buf);
	ERRCHECK(push_buf(&out_msg->data, sizeof(size_t), &count));
	out_msg->type |= MESSAGE_OP_SUCC;

	do_log(log, *conn, STRING_READN_FILE, "none", "Success.");
	return 0;
}