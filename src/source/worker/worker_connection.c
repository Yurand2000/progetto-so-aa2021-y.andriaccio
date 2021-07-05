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

int do_acceptor(int* acceptor, int* newconn, log_t* log, shared_state* shared)
{
	net_msg in_msg, out_msg;

	ERRCHECK(pthread_mutex_lock(&shared->state_mux));
	int curr_conns = shared->current_conns;
	ERRCHECK(pthread_mutex_unlock(&shared->state_mux));
	if (curr_conns >= shared->ro_max_conns)
	{
		do_log(log, -1, STRING_OPEN_CONN, "none", "Max clients connected.");
		return 0;
	}

	ERRCHECK(pthread_mutex_lock(&shared->state_mux));
	ERRCHECK((*newconn = accept(*acceptor, NULL, 0)));
	ERRCHECK(pthread_mutex_unlock(&shared->state_mux));

	//waiting for a message from the new connection, otherwise drop it.
	//need a timeout!!
	create_message(&in_msg);
	ERRCHECKDO(read_msg(*newconn, &in_msg), destroy_message(&in_msg));
	create_message(&out_msg);

	if (is_client_message(&in_msg, MESSAGE_OPEN_CONN) == 0)
	{
		if (add_client(shared, *newconn) == -1)
		{
			close(*newconn); *newconn = -1;
			do_log(log, *newconn, STRING_OPEN_CONN, "none", "Max clients connected. [2]");
		}
		else
		{
			out_msg.type = MESSAGE_OCONN_ACK;
			set_checksum(&out_msg);
			ERRCHECK(write_msg(*newconn, &out_msg));

			do_log(log, *newconn, STRING_OPEN_CONN, "none", "Client connected.");
		}
	}
	else
	{
		close(*newconn); *newconn = -1;

		do_log(log, -1, STRING_OPEN_CONN, "none", "Unknown client tried to connect.");
	}

	destroy_message(&in_msg);
	destroy_message(&out_msg);
	return 0;
}

int do_close_connection(int* conn, net_msg* in_msg, net_msg* out_msg,
	file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname,
	shared_state* state)
{
	out_msg->type = MESSAGE_CCONN_ACK;

	//unown any locked files and close them
	for (size_t i = 0; i < file_num; i++)
	{
		if (is_locked_file(&files[i], *conn) == 0)
		{
			long diff;
			close_file(&files[i], *conn, &diff);

			ERRCHECK(pthread_mutex_lock(&state->state_mux));
			state->current_storage -= diff;
			ERRCHECK(pthread_mutex_unlock(&state->state_mux));
		}
	}

	set_checksum(out_msg);
	ERRCHECK(write_msg(*conn, out_msg));

	do_log(log, *conn, STRING_CLOSE_CONN, "none", "Connection closed");
	ERRCHECK(remove_client(state, *conn));

	//close the connection
	close(*conn);
	*conn = -1;
	return 0;
}