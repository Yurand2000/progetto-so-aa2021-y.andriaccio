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

static int _open_file_create(int thread_id, int fslot, char* name, int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state);

static int _open_file_open(int thread_id, int fslot, char* name, int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state);

int do_open_file(int thread_id, int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state)
{
	char name[FILE_NAME_MAX_SIZE];
	ERRCHECK(read_file_name(in_msg, name, FILE_NAME_MAX_SIZE));

	out_msg->type = MESSAGE_OFILE_ACK;
	SET_EMPTY_STRING(lastop_writefile_pname);

	int fex = get_file(files, file_num, name);
	if (fex == -1 && errno != ENOENT)
	{
		do_log(log, thread_id, *conn, name, STRING_OPEN_FILE, "File error.", 0, 0);
		return -1;
	}

	if (GETFLAGS(in_msg->type) & MESSAGE_OPEN_OCREATE)	//create file
		return _open_file_create(thread_id, fex, name, conn, in_msg, out_msg, files, file_num,
			log, lastop_writefile_pname, state);
	else //open file
		return _open_file_open(thread_id, fex, name, conn, in_msg, out_msg, files, file_num,
			log, lastop_writefile_pname, state);
}

static int _open_file_create(int thread_id, int fslot, char* name, int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state)
{
	int owner = (GETFLAGS(in_msg->type) & MESSAGE_OPEN_OLOCK) ? *conn : OWNER_NULL;	//want to lock?

	if (fslot == -1)
	{
		int nof = get_empty_slot(files, file_num);
		if (nof == -1 && errno != EMFILE) return -1;

		if (nof == -1)
		{
			//max files in memory!
			out_msg->type |= MESSAGE_FILE_ERRMAXFILES;

			do_log(log, thread_id, *conn, name, STRING_CREATE_FILE, "Too many open files.", 0, 0);
		}
		else
		{
			int res = create_file_struct(&files[nof], name, owner);
			if (res == -1)
			{
				//file error!
				do_log(log, thread_id, *conn, name, STRING_CREATE_FILE, "File error.", 0, 0);
				return -1;
			}
			else
			{
				ERRCHECK(pthread_mutex_lock(&state->state_mux));
				state->current_files++;
				if (state->current_files > state->max_reached_files)
					state->max_reached_files = state->current_files;
				ERRCHECK(pthread_mutex_unlock(&state->state_mux));

				int opn = open_file(&files[nof], owner);
				if (opn == 0)
				{
					out_msg->type |= MESSAGE_OP_SUCC;

					if (GETFLAGS(in_msg->type) & MESSAGE_OPEN_OLOCK)
						strncpy(lastop_writefile_pname, name, FILE_NAME_MAX_SIZE);

					if(owner == OWNER_NULL)
						do_log(log, thread_id, *conn, name, STRING_CREATE_FILE, "Success.", 0, 0);
					else
						do_log(log, thread_id, *conn, name, STRING_CREATELOCK_FILE, "Success.", 0, 0);
				}
				else
				{
					//file error!
					do_log(log, thread_id, *conn, name, STRING_CREATE_FILE, "File error.", 0, 0);
					return -1;
				}
			}
		}
	}
	else
	{
		//file already exists;
		out_msg->type |= MESSAGE_FILE_EXISTS;

		do_log(log, thread_id, *conn, name, STRING_CREATE_FILE, "File already exists.", 0, 0);
	}
	return 0;
}

static int _open_file_open(int thread_id, int fslot, char* name, int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state)
{
	int owner = (GETFLAGS(in_msg->type) & MESSAGE_OPEN_OLOCK) ? *conn : OWNER_NULL;	//want to lock?

	if (fslot >= 0)
	{
		//file exists
		int opn = open_file(&files[fslot], owner);
		if (opn == 0)
		{
			out_msg->type |= MESSAGE_OP_SUCC;

			if (owner == OWNER_NULL)
				do_log(log, thread_id, *conn, name, STRING_OPEN_FILE, "Success.", 0, 0);
			else
				do_log(log, thread_id, *conn, name, STRING_OPENLOCK_FILE, "Success.", 0, 0);
		}
		else if (errno == EAGAIN)
		{
			out_msg->type |= MESSAGE_FILE_NOWN;

			do_log(log, thread_id, *conn, name, STRING_OPEN_FILE, "Permission denied.", 0, 0);
		}
		else
		{
			//file error!
			do_log(log, thread_id, *conn, name, STRING_OPEN_FILE, "File error.", 0, 0);
			return -1;
		}
	}
	else
	{
		//file doesn't exist
		out_msg->type |= MESSAGE_FILE_NEXISTS;

		do_log(log, thread_id, *conn, name, STRING_OPEN_FILE, "File doesn't exist.", 0, 0);
	}
	return 0;
}