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

int prepare_shared_data(shared_state* state, int socket, cfg_t* config)
{
	return init_shared_state(state, config->algorithm, config->storage_capacity,
		config->max_files, config->max_connections, socket, pthread_self());
}

int prepare_threads(pthread_t** threads, size_t* threads_count, worker_data** work_data,
	file_t* files, size_t file_num,
	shared_state* state, log_t* log, cfg_t* config)
{
	*threads_count = config->worker_threads;
	MALLOC(*threads, sizeof(pthread_t) * *threads_count);
	MALLOC(*work_data, sizeof(worker_data) * *threads_count);
	for (size_t i = 0; i < *threads_count; i++)
	{
		init_worker_data(&(*work_data)[i], files, file_num, log, state);

		//spawn thread
		ERRCHECK(pthread_create(&(*threads)[i], NULL, worker_routine, &(*work_data)[i]));
	}
	return 0;
}