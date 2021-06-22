#ifndef SERVER_SHARED_STATE
#define SERVER_SHARED_STATE
#include "win32defs.h"

#include <pthread.h>
#include <stdlib.h>

#include "file.h"

typedef struct shared_state
{
	//readonly data
	char ro_cache_miss_algorithm;
	long ro_max_storage;
	int ro_max_files;
	int ro_max_conns;
	int ro_acceptor_fd;

	//mux shared by all threads
	pthread_mutex_t state_mux;
	client_state* clients;
	long current_storage;
	int current_files;
	int current_conns;
	long max_reached_storage;
	int max_reached_files;
	int max_reached_conns;
	int cache_miss_execs;
	size_t last_evicted;
} shared_state;

typedef struct client_state {
	int fd;
	char lastop[FILE_NAME_MAX_SIZE];
} client_state;

int init_shared_state(shared_state* ss, char cache_miss, long max_storage,
	int max_files, int max_conns, int acceptor);
int destroy_shared_state(shared_state* ss);

int add_client(shared_state* ss, int conn);
int remove_client(shared_state* ss, int conn);
int get_client_lastop(shared_state* ss, int conn, char** out);

#endif