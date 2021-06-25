#include "worker.h"
#include "worker/worker_generics.h"

#include <stdint.h>
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

#include "errset.h"
#include "net_msg.h"
#include "message_type.h"
#include "net_msg_macros.h"

#define THREAD_ERRCHECK(cond) if(cond == -1) return (void*)0;

int init_worker_data(worker_data* wd, file_t* files, size_t file_num,
	log_t* log, shared_state* shared)
{
	if (wd == NULL || files == NULL || log == NULL || shared == NULL || file_num == 0) ERRSET(EINVAL, -1);

	ERRCHECK(pthread_mutex_init(&wd->thread_mux, NULL));
	ERRCHECK(pthread_cond_init(&wd->thread_cond, NULL));
	wd->do_work = WORKER_IDLE;
	wd->exit = 0;
	wd->in_conn = wd->add_conn = -1;

	wd->files = files;
	wd->file_num = file_num;
	wd->log = log;

	wd->shared = shared;
	return 0;
}

int destroy_worker_data(worker_data* wd)
{
	if (wd == NULL) ERRSET(EINVAL, -1);

	ERRCHECK(pthread_mutex_destroy(&wd->thread_mux));
	ERRCHECK(pthread_cond_destroy(&wd->thread_cond));
	return 0;
}

//-----------------------------------------------------------------------------
static int worker_do(int* conn, int* newconn, file_t* files, size_t file_num,
	log_t* log, shared_state* shared);

void* worker_routine(void* args)
{
	worker_data* data = (worker_data*)args;
	int worker_did = 0;
	while(1)
	{
		THREAD_ERRCHECK(pthread_mutex_lock(&data->thread_mux));
		while (data->do_work != WORKER_DO && !data->exit)
			THREAD_ERRCHECK(pthread_cond_wait(&data->thread_cond, &data->thread_mux));

		if (data->exit)
		{
			THREAD_ERRCHECK(pthread_mutex_unlock(&data->thread_mux));
			return (void*)((uintptr_t)worker_did);
		}
		else
		{
			int conn = data->in_conn; int newconn = -1;
			THREAD_ERRCHECK(pthread_mutex_unlock(&data->thread_mux));

			THREAD_ERRCHECK(worker_do(&conn, &newconn, data->files,
				data->file_num, data->log, data->shared));
			worker_did++;

			THREAD_ERRCHECK(pthread_mutex_lock(&data->thread_mux));
			data->in_conn = conn;
			data->add_conn = newconn;
			data->do_work = WORKER_DONE;
			THREAD_ERRCHECK(pthread_mutex_unlock(&data->thread_mux));

			THREAD_ERRCHECK(raise(SIGUSR1));	//tell the main thread it has finished working
		}
	}
}

/* this is an internal function used by the worker to handle a request.
 * when returning, the values of conn and newconn tell the main thread if a
 * connection has been added or dropped.
 * lastop_writefile_pname contains the pathname of the file if the last operation
 * was an open with O_CREATE and O_LOCK flags and had success. */
static int worker_do(int* conn, int* newconn, file_t* files, size_t file_num,
	log_t* log, shared_state* shared)
{
	net_msg in_msg, out_msg; int ret; char* lastop;

	if (*conn == shared->ro_acceptor_fd)
		return do_acceptor(conn, newconn, log, shared);

	//read message from connection
	create_message(&in_msg);
	ERRCHECKDO(read_msg(*conn, &in_msg), { destroy_message(&in_msg); });

	ret = is_client_message(&in_msg, in_msg.type);
	if (ret != 0) ERRSETDO(EBADMSG, { destroy_message(&in_msg); }, -1);
	create_message(&out_msg);

	ERRCHECK(get_client_lastop(shared, *conn, &lastop));
	if(in_msg.type & MESSAGE_CLOSE_CONN)
		ret = do_close_connection(conn, &in_msg, &out_msg, files, file_num, log, lastop, shared);

	else if(in_msg.type & MESSAGE_OPEN_FILE)
		ret = do_open_file(conn, &in_msg, &out_msg, files, file_num, log, lastop, shared);

	else if(in_msg.type & MESSAGE_CLOSE_FILE)
		ret = do_close_file(conn, &in_msg, &out_msg, files, file_num, log, lastop, shared);

	else if(in_msg.type & MESSAGE_READ_FILE)
		ret = do_read_file(conn, &in_msg, &out_msg, files, file_num, log, lastop, shared);

	else if (in_msg.type & MESSAGE_READN_FILE)
		ret = do_readn_files(conn, &in_msg, &out_msg, files, file_num, log, lastop, shared);

	else if(in_msg.type & MESSAGE_WRITE_FILE)
		ret = do_write_file(conn, &in_msg, &out_msg, files, file_num, log, lastop, shared);

	else if(in_msg.type & MESSAGE_APPEND_FILE)
		ret = do_append_file(conn, &in_msg, &out_msg, files, file_num, log, lastop, shared);

	else if(in_msg.type & MESSAGE_LOCK_FILE)
		ret = do_lock_file(conn, &in_msg, &out_msg, files, file_num, log, lastop, shared);

	else if(in_msg.type & MESSAGE_UNLOCK_FILE)
		ret = do_unlock_file(conn, &in_msg, &out_msg, files, file_num, log, lastop, shared);

	else if(in_msg.type & MESSAGE_REMOVE_FILE)
		ret = do_remove_file(conn, &in_msg, &out_msg, files, file_num, log, lastop, shared);

	else
	{
		out_msg.type = MESSAGE_NULL;
		do_log(log, *conn, "Unknown Operation", "none", "Unknown Operation");
	}

	set_checksum(&out_msg);
	ERRCHECK(write_msg(*conn, &out_msg));
	destroy_message(&out_msg);
	destroy_message(&in_msg);
	return ret;
}