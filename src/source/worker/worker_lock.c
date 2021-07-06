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
#include "../databuf.h"

int do_lock_file(int thread_id, int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state)
{
	char name[FILE_NAME_MAX_SIZE];
	ERRCHECK(read_file_name(in_msg, name, FILE_NAME_MAX_SIZE));
	SET_EMPTY_STRING(lastop_writefile_pname);

	out_msg->type = MESSAGE_LFILE_ACK;

	int fex = get_file(files, file_num, name);
	if (fex >= 0)
	{
		int lck = lock_file(&files[fex], *conn);
		if (lck == 0)
		{
			out_msg->type |= MESSAGE_OP_SUCC;

			do_log(log, thread_id, *conn, name, STRING_LOCK_FILE, "Success.", 0, 0);
		}
		else if (errno == EPERM)
		{
			out_msg->type |= MESSAGE_FILE_NOPEN;

			do_log(log, thread_id, *conn, name, STRING_LOCK_FILE, "File is closed.", 0, 0);
		}
		else if (errno == EAGAIN)
		{
			out_msg->type |= MESSAGE_FILE_NOWN;
			do_log(log, thread_id, *conn, name, STRING_LOCK_FILE, "Permission denied.", 0, 0);
		}
		else
		{
			do_log(log, thread_id, *conn, name, STRING_LOCK_FILE, "File error.", 0, 0);
			return -1;
		}
	}
	else if (errno == ENOENT)
	{
		out_msg->type |= MESSAGE_FILE_NEXISTS;

		do_log(log, thread_id, *conn, name, STRING_LOCK_FILE, "File doesn't exist.", 0, 0);
	}
	else
	{
		do_log(log, thread_id, *conn, name, STRING_LOCK_FILE, "File error.", 0, 0);
		return -1;
	}
	return 0;
}

int do_unlock_file(int thread_id, int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state)
{
	char name[FILE_NAME_MAX_SIZE];
	ERRCHECK(read_file_name(in_msg, name, FILE_NAME_MAX_SIZE));
	SET_EMPTY_STRING(lastop_writefile_pname);

	out_msg->type = MESSAGE_ULFILE_ACK;
	int fex = get_file(files, file_num, name);
	if (fex >= 0)
	{
		int lck = unlock_file(&files[fex], *conn);
		if (lck == 0)
		{
			out_msg->type |= MESSAGE_OP_SUCC;

			do_log(log, thread_id, *conn, name, STRING_UNLOCK_FILE, "Success.", 0, 0);
		}
		else if (errno == EPERM)
		{
			out_msg->type |= MESSAGE_FILE_NOPEN;
			do_log(log, thread_id, *conn, name, STRING_UNLOCK_FILE, "File is closed.", 0, 0);
		}
		else if (errno == EAGAIN)
		{
			out_msg->type |= MESSAGE_FILE_NOWN;
			do_log(log, thread_id, *conn, name, STRING_UNLOCK_FILE, "Permission denied.", 0, 0);
		}
		else
		{
			do_log(log, thread_id, *conn, name, STRING_UNLOCK_FILE, "File error.", 0, 0);
			return -1;
		}
	}
	else if (errno == ENOENT)
	{
		out_msg->type |= MESSAGE_FILE_NEXISTS;

		do_log(log, thread_id, *conn, name, STRING_UNLOCK_FILE, "File doesn't exist.", 0, 0);
	}
	else
	{
		do_log(log, thread_id, *conn, name, STRING_UNLOCK_FILE, "File error.", 0, 0);
		return -1;
	}
	return 0;
}