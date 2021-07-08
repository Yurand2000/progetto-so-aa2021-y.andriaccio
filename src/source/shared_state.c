#include "shared_state.h"
#include "win32defs.h"

#include <pthread.h>
#include <stdlib.h>

#include "errset.h"

int init_shared_state(shared_state* ss, char cache_miss, long max_storage,
	int max_files, int max_conns, int acceptor, pthread_t main_thread)
{
	if (ss == NULL) ERRSET(EINVAL, -1);

	ss->ro_cache_miss_algorithm = cache_miss;
	ss->ro_max_storage = max_storage;
	ss->ro_max_files = max_files;
	ss->ro_max_conns = max_conns;
	ss->ro_acceptor_fd = acceptor;
	ss->ro_main_thread = main_thread;

	ERRCHECK(pthread_mutex_init(&ss->state_mux, NULL));
	MALLOC(ss->clients, sizeof(client_state) * ss->ro_max_conns);
	for (size_t i = 0; i < ss->ro_max_conns; i++)
	{
		ss->clients[i].fd = -1;
		ss->clients[i].lastop[0] = '\0';
	}
	ss->current_storage = 0;
	ss->current_files = 0;
	ss->current_conns = 0;
	ss->max_reached_storage = 0;
	ss->max_reached_files = 0;
	ss->max_reached_conns = 0;
	ss->cache_miss_execs = 0;
	ss->lru_clock_pos = 0;
	return 0;
}

int destroy_shared_state(shared_state* ss)
{
	if (ss == NULL) ERRSET(EINVAL, -1);

	free(ss->clients);
	ERRCHECK(pthread_mutex_destroy(&ss->state_mux));
	return 0;
}

int add_client(shared_state* ss, int conn)
{
	if (ss == NULL || conn < 0) ERRSET(EINVAL, -1);
	int assign = 0;

	ERRCHECK(pthread_mutex_lock(&ss->state_mux));
	if (ss->current_conns < ss->ro_max_conns)
	{
		for (size_t i = 0; i < ss->ro_max_conns && assign == 0; i++)
		{
			if (ss->clients[i].fd == -1)
			{
				ss->clients[i].fd = conn;
				ss->clients[i].lastop[0] = '\0';

				ss->current_conns++;
				if (ss->current_conns > ss->max_reached_conns)
					ss->max_reached_conns = ss->current_conns;
				assign = 1;
			}
		}
	}
	ERRCHECK(pthread_mutex_unlock(&ss->state_mux));

	if (assign != 0)
		return 0;
	else
		ERRSET(ENFILE, -1);
}

int remove_client(shared_state* ss, int conn)
{
	if (ss == NULL || conn < 0) ERRSET(EINVAL, -1);
	int assign = 0;

	ERRCHECK(pthread_mutex_lock(&ss->state_mux));
	if (ss->current_conns > 0)
	{
		for (size_t i = 0; i < ss->ro_max_conns && assign == 0; i++)
		{
			if (ss->clients[i].fd == conn)
			{
				ss->clients[i].fd = -1;
				ss->current_conns--;
				assign = 1;
			}
		}
	}
	ERRCHECK(pthread_mutex_unlock(&ss->state_mux));

	if (assign != 0)
		return 0;
	else
		ERRSET(ENOENT, -1);
}

int get_client_lastop(shared_state* ss, int conn, char** out)
{
	if (ss == NULL || conn < 0 || out == NULL) ERRSET(EINVAL, -1);
	*out = NULL;

	ERRCHECK(pthread_mutex_lock(&ss->state_mux));
	for (size_t i = 0; i < ss->ro_max_conns && *out == NULL; i++)
	{
		if (ss->clients[i].fd == conn)
			*out = ss->clients[i].lastop;
	}
	ERRCHECK(pthread_mutex_unlock(&ss->state_mux));

	if (*out != NULL)
		return 0;
	else
		ERRSET(ENOENT, -1);
}