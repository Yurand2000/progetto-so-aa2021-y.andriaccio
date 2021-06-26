#include "server_subcalls.h"
#include "../win32defs.h"

#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/signalfd.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../errset.h"
#include "../net_msg_macros.h"
#include "../config.h"
#include "../log.h"
#include "../worker.h"
#include "../readn_writen.h"

int sighup_handler(worker_data* work_data, pthread_t* threads, size_t threads_size,
	struct pollfd* poll_array, nfds_t poll_size, int* exit)
{
	//set all threads to stop
	for (size_t i = 0; i < threads_size; i++)
	{
		worker_data* th_data = &work_data[i];
		ERRCHECK(pthread_mutex_lock(&th_data->thread_mux));
		th_data->exit = 1;
		ERRCHECK(pthread_mutex_unlock(&th_data->thread_mux));
	}

	//close signal handler
	ERRCHECK(close(poll_array[poll_size - 1].fd));

	//join threads
	ERRCHECK(stop_all_threads(work_data, threads_size));
	ERRCHECK(join_all_threads(threads, threads_size));
	*exit = 1;
	return 0;
}

int poll_call(struct pollfd* poll_array, nfds_t poll_size,
	int working_threads, size_t threads_count)
{
	if (working_threads < (int)threads_count)
	{
		ERRCHECK(poll(poll_array, poll_size, -1)); //blocking
	}
	else
	{
		//threads are all working, just listen to signals.
		ERRCHECK(poll(&poll_array[poll_size - 1], 1, -1));
	}
	return 0;
}

int after_poll_signal(struct pollfd* poll_array, nfds_t poll_size, pthread_t* threads,
	worker_data* threads_data, size_t threads_count, int acceptor,
	int* working_threads, int* exit, int* sighup, cfg_t* config)
{
	struct signalfd_siginfo in_signal;
	ERRCHECK(readn(poll_array[poll_size - 1].fd, &in_signal, sizeof(struct signalfd_siginfo)));
	if (in_signal.ssi_signo == SIGINT || in_signal.ssi_signo == SIGQUIT)
	{
		//close acceptor connection and signal handler
		ERRCHECK(close(poll_array[poll_size - 1].fd));
		ERRCHECK(close(poll_array[poll_size - 2].fd));

		ERRCHECK(stop_all_threads(threads_data, threads_count));
		ERRCHECK(join_all_threads(threads, threads_count));
		ERRCHECK(close_all_client_connections(poll_array, config));
		*exit = 1;
	}
	else if (in_signal.ssi_signo == SIGHUP)
	{
		ERRCHECK(sighup_do(poll_array, poll_size, sighup));
	}
	else if (in_signal.ssi_signo == SIGUSR1)
	{
		ERRCHECK(thread_has_finished(poll_array, poll_size, threads_data,
			threads_count, acceptor, working_threads, config));
	}
	return 0;
}

int after_poll_acceptor(struct pollfd* poll_array, nfds_t poll_size, pthread_t* threads,
	worker_data* threads_data, size_t threads_count, int acceptor,
	int* working_threads, int* exit, int* sighup, cfg_t* config)
{
	int fd = poll_array[poll_size - 2].fd;
	poll_array[poll_size - 2].fd = -1;

	ERRCHECK(thread_assign_work(threads_data, threads_count, fd, working_threads));
	return 0;
}

int after_poll_connection(struct pollfd* poll_array, nfds_t poll_size, pthread_t* threads,
	worker_data* threads_data, size_t threads_count, int acceptor,
	int* working_threads, int* exit, int* sighup, cfg_t* config)
{
	int fd = -1;
	//find the first connection who needs attention
	for (size_t i = 0; i < config->max_connections && fd == -1; i++)
	{
		if (poll_array[i].revents & POLLIN)
		{
			fd = poll_array[i].fd;
			poll_array[i].fd = -1;
		}
	}
	if(fd == -1) ERRSET(EIO, -1);

	ERRCHECK(thread_assign_work(threads_data, threads_count, fd, working_threads));
	return 0;
}

int thread_assign_work(worker_data* threads_data,
	size_t threads_count, int work_conn, int* working_threads)
{
	int done = 0;
	for (size_t i = 0; i < threads_count && !done; i++)
	{
		worker_data* th_data = &threads_data[i];
		ERRCHECK(pthread_mutex_lock(&th_data->thread_mux));
		if (th_data->do_work == WORKER_IDLE)
		{
			th_data->do_work = WORKER_DO;
			th_data->in_conn = work_conn;
			ERRCHECK(pthread_cond_signal(&th_data->thread_cond));
			(*working_threads)++;
			done = 1;
		}
		ERRCHECK(pthread_mutex_unlock(&th_data->thread_mux));
	}
	if (done == 0) return -1;
	else return 0;
}

int sighup_do(struct pollfd* poll_array, nfds_t poll_size, int* sighup)
{
	ERRCHECK(close(poll_array[poll_size - 2].fd));
	*sighup = 1;
	return 0;
}

int thread_has_finished(struct pollfd* poll_array, nfds_t poll_size,
	worker_data* threads_data, size_t threads_count, int acceptor,
	int* working_threads, cfg_t* config)
{
	int work_conn, new_conn, done = 0;
	for (size_t i = 0; i < threads_count; i++)
	{
		worker_data* th_data = &threads_data[i];
		ERRCHECK(pthread_mutex_lock(&th_data->thread_mux));
		if (th_data->do_work == WORKER_DONE)
		{
			th_data->do_work = WORKER_IDLE;
			work_conn = th_data->in_conn;
			new_conn = th_data->add_conn;
			done = 1;
			(*working_threads)--;
		}
		ERRCHECK(pthread_mutex_unlock(&th_data->thread_mux));

		if (done)
		{
			//if work_conn == acceptor, add the new connection
			if (work_conn == acceptor)
				poll_array[poll_size - 2].fd = work_conn;
			else if (work_conn > 0) //readd the connection to the poll list
				ERRCHECK(add_connection_to_poll(poll_array, poll_size, work_conn, config));

			if (new_conn > 0) //new connection incoming
				ERRCHECK(add_connection_to_poll(poll_array, poll_size, new_conn, config));

			done = 0;
		}
	}
	return 0;
}