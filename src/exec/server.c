#include "server.h"
#include "../source/win32defs.h"

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

#include "../source/errset.h"
#include "../source/net_msg_macros.h"
#include "../source/config.h"
#include "../source/log.h"
#include "../source/worker.h"
#include "../source/readn_writen.h"
#include "../source/server/server_subcalls.h"
#include "../source/server/server_compression.h"

int main(int argc, char* argv[])
{
	cfg_t config_data;
	log_t log_file;

	//parse command line and load config file, start logging ------------------
	if (parse_cmd_start_log(argc, argv, &config_data, &log_file) == -1)
	{
		if (errno != EINVAL) { perror("Startup error"); return 1; }
		else return 0;
	}
	printf("* * * SERVER STARTED * * *\n");
	//prepare compression library ---------------------------------------------
	PERRCHECK(compression_init(), "Compression library error");

	//prepare socket and start listening --------------------------------------
	int sck;
	PERRCHECK(prepare_socket(&sck, &config_data), "Socket initialization error");

	//prepare connection storage for poll + signal handlers -------------------
	int sigfd; sigset_t old_mask;
	PERRCHECK(signal_to_file(&sigfd, &old_mask), "Signal to file handler error");

	struct pollfd* poll_array; nfds_t poll_size;
	PERRCHECK(initialize_poll_array(sck, sigfd, &poll_array, &poll_size, &config_data),
		"Poll array creation error");

	file_t* files; size_t file_num;
	PERRCHECK(initialize_file_structure(&files, &file_num, &config_data), "File initialization error");

	shared_state state;
	PERRCHECK(prepare_shared_data(&state, sck, &config_data), "Shared State initialization error");

	//prepare threads
	pthread_t* threads; worker_data* threads_data; size_t threads_count; int working_threads = 0;
	PERRCHECK(prepare_threads(&threads, &threads_count, &threads_data,
		files, file_num, &state, &log_file, &config_data), "Thread initialization error");

	//poll --------------------------------------------------------------------
	int exit = 0; int sighup = 0; int current_connections;
	while (!exit)
	{
		PERRCHECK(get_current_connections(&state, &current_connections),
			"Poll loop error");

		//if SIGHUP was raised and there are no more active clients
		if (sighup && current_connections == 0)
		{
			PERRCHECK(sighup_handler(threads_data, threads, threads_count, poll_array, poll_size, &exit),
				"SigHUP terminator error");
		}
		else
		{
			PERRCHECK(poll_call(poll_array, poll_size, working_threads, threads_count),
				"Poll call error");

			if (poll_array[poll_size - 1].revents & POLLIN) //check signals
			{
				PERRCHECK(after_poll_signal(poll_array, poll_size, threads,
					threads_data, threads_count,
					sck, &working_threads, &exit, &sighup, &config_data, &state),
					"Signal handling error");
			}
			else if (poll_array[poll_size - 2].revents & POLLIN) //check acceptor
			{
				PERRCHECK(after_poll_acceptor(poll_array, poll_size, threads,
					threads_data, threads_count,
					sck, &working_threads, &exit, &sighup, &config_data),
					"Acceptor handling error");
			}
			else //check connections
			{
				PERRCHECK(after_poll_connection(poll_array, poll_size, threads,
					threads_data, threads_count,
					sck, &working_threads, &exit, &sighup, &config_data),
					"Connection handling error");
			}
		}
	}

	//print stats
	printf("* * * SERVER TERMINATED * * *\n");
	PERRCHECK(print_stats(&log_file, &state, files, file_num), "Statistics printer error");

	//destruction -------------------------------------------------------------
	free(poll_array);

	//destroy thread data structs
	for (size_t i = 0; i < threads_count; i++)
		destroy_worker_data(&threads_data[i]);
	free(threads_data);
	free(threads);

	//destroy generic state
	destroy_shared_state(&state);

	//destroy files
	for(size_t i = 0; i < file_num; i++)
		destroy_file_struct(&files[i]);
	free(files);

	close(sigfd);
	close(sck);
	unlink(config_data.socket_file);

	destroy_log_file_struct(&log_file);

	PERRCHECK(compression_destroy(), "Compression library error [2]");
	return 0;
}