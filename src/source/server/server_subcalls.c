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

int get_current_connections(shared_state* state, int* curr)
{
	ERRCHECK(pthread_mutex_lock(&state->state_mux));
	*curr = state->current_conns;
	ERRCHECK(pthread_mutex_unlock(&state->state_mux));
	return 0;
}

int add_connection_to_poll(struct pollfd* poll_array, nfds_t poll_size, int conn, cfg_t* config)
{
	int done = 0;
	for (size_t i = 0; i < config->max_connections && !done; i++)
	{
		if (poll_array[i].fd == -1)
		{
			poll_array[i].fd = conn;
			done = 1;
		}
	}
	return 0;
}

int stop_all_threads(worker_data* threads_data, size_t threads_count)
{
	for (size_t i = 0; i < threads_count; i++)
	{
		worker_data* th_data = &threads_data[i];
		ERRCHECK(pthread_mutex_lock(&th_data->thread_mux));
		th_data->exit = 1;
		if (th_data->do_work == WORKER_IDLE)
			ERRCHECK(pthread_cond_signal(&th_data->thread_cond));
		ERRCHECK(pthread_mutex_unlock(&th_data->thread_mux));
	}
	return 0;
}

int join_all_threads(pthread_t* threads, size_t threads_count)
{
	for (size_t i = 0; i < threads_count; i++)
		ERRCHECK(pthread_join(threads[i], NULL));
	return 0;
}

int close_all_client_connections(struct pollfd* poll_array, cfg_t* config)
{
	for (nfds_t i = 0; i < (nfds_t)config->max_connections; i++)
	{
		if (poll_array[i].fd != -1)
			ERRCHECK(close(poll_array[i].fd));
	}
	return 0;
}