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

int do_close_file(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state)
{
	char name[FILE_NAME_MAX_SIZE]; size_t name_size;
	ERRCHECK(read_file_name(in_msg, name, name_size));

	SET_EMPTY_STRING(lastop_writefile_pname);
	out_msg->type = MESSAGE_CFILE_ACK;

	int fex = get_file(files, file_num, name);
	if (fex >= 0)
	{
		long diff;
		int lck = close_file(&files[fex], *conn, &diff);
		if (lck == 0)
		{
			out_msg->type |= MESSAGE_OP_SUCC;

			ERRCHECK(pthread_mutex_lock(&state->state_mux));
			state->current_storage -= diff;
			ERRCHECK(pthread_mutex_unlock(&state->state_mux));

			do_log(log, *conn, STRING_CLOSE_FILE, name, "Success.");
		}
		else if (errno == EPERM)
		{
			out_msg->type |= MESSAGE_FILE_LOCK;

			do_log(log, *conn, STRING_CLOSE_FILE, name, "Permission denied.");
		}
		else
		{
			do_log(log, *conn, STRING_CLOSE_FILE, name, "File error.");
			return -1;
		}
	}
	else if (errno == ENOENT)
	{
		out_msg->type |= MESSAGE_FILE_NEXISTS;

		do_log(log, *conn, STRING_CLOSE_FILE, name, "File does not exist.");
	}
	else
	{
		do_log(log, *conn, STRING_CLOSE_FILE, name, "File error.");
		return -1;
	}
	return 0;
}