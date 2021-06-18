#include "server.h"
#include "../source/win32defs.h"

#include <unistd.h>
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

int main(int argc, char* argv[])
{
	cfg_t config_data;
	log_t log_file;

	//parse command line and load config file, start logging ------------------
	command_line_parsing(argc, argv, &config_data);
	init_log_file_struct(&log_file, config_data.log_file);
	reset_log(&log_file);
	printf("* * * SERVER STARTED * * *\n");

	//prepare socket and start listening --------------------------------------
	size_t len; SOCKNAME_VALID(config_data.socket_file, &len);
	ERRCHECK(unlink(config_data.socket_file));
	int sck; ERRCHECK((sck = socket(AF_UNIX, SOCK_STREAM, 0)));

	//generate connection address
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, config_data.socket_file, len + 1);

	//bind to listen
	ERRCHECK( bind(sck, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) );
	ERRCHECK( listen(sck, config_data.max_connections) );

	//prepare connection storage for poll + signal handlers -------------------

	//signal handler to file descriptor
	sigset_t sig_mask, old_mask; struct signalfd_siginfo sig_read;
	ERRCHECK(sigemptyset(&sig_mask));
	ERRCHECK(sigaddset(&sig_mask, SIGINT));
	ERRCHECK(sigaddset(&sig_mask, SIGQUIT));
	ERRCHECK(sigaddset(&sig_mask, SIGHUP));
	ERRCHECK(sigaddset(&sig_mask, SIGUSR1)); //triggered when thread finish
	//ignore signals, those are delivered only to the signal file descriptor.
	pthread_sigmask(SIG_SETMASK, &sig_mask, &old_mask);
	int sigfd = signalfd(-1, &sig_mask, 0); //create file descriptor

	//generate poll array
	struct pollfd* poll_array; nfds_t poll_size = config_data.max_connections + 2;
	MALLOC(poll_array, poll_size * sizeof(struct pollfd));

	//fill poll array
	for (size_t i = 0; i < config_data.max_connections; i++)
	{
		poll_array[i].fd = -1;
		poll_array[i].events = POLLIN;
	}
	poll_array[poll_size - 2].fd = sck; //listener
	poll_array[poll_size - 2].events = POLLIN;
	poll_array[poll_size - 1].fd = sigfd; //signal listener
	poll_array[poll_size - 1].events = POLLIN;

	//prepare the file structure and generic state mux
	file_t* files; size_t file_num = config_data.max_files;
	long current_storage = 0;
	int current_files = 0;
	pthread_mutex_t state_mux;
	MALLOC(files, sizeof(file_t) * file_num);
	ERRCHECK(pthread_mutex_init(&state_mux, NULL));

	//prepare threads
	pthread_t* threads; worker_data* threads_data;
	size_t threads_count = config_data.worker_threads; int working_threads = 0;
	MALLOC(threads, sizeof(pthread_t) * threads_count);
	MALLOC(threads_data, sizeof(worker_data) * threads_count);
	for (size_t i = 0; i < threads_count; i++)
	{
		//fill each worker_data for each thread - - - - - - - - - - - - - - - -
		worker_data* th_data = &threads_data[i];

		ERRCHECK(pthread_mutex_init(&th_data->thread_mux, NULL));
		ERRCHECK(pthread_cond_init(th_data->thread_cond, NULL));
		th_data->do_work = 0;
		th_data->exit = 0;
		th_data->work_conn = 0;
		th_data->new_conn = 0;
		th_data->work_conn_lastop = NULL;

		th_data->files = files;
		th_data->file_num = file_num;
		th_data->log = &log_file;

		th_data->max_storage = config_data.storage_capacity;
		th_data->max_files = config_data.max_files;
		th_data->cache_miss_algorithm = config_data.algorithm;
		th_data->current_storage = &current_storage;
		th_data->current_files = &current_files;
		th_data->state_mux = &state_mux;

		//spawn thread - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		ERRCHECK(pthread_create(&threads[i], NULL, worker_routine, &threads_data[i]));
	}

	//poll --------------------------------------------------------------------
	int exit = 0; int hup = 0;
	while (!exit)
	{
		//if SIGHUP was raised and there are no more active clients
		if (hup && current_connections == 0)
		{
			//join all threads, close all connections, exit ---------------
			//set all threads to stop
			for (size_t i = 0; i < threads_count; i++)
			{
				worker_data* th_data = &threads_data[i];
				ERRCHECK(pthread_mutex_lock(&th_data->thread_mux));
				th_data->exit = 1;
				ERRCHECK(pthread_mutex_unlock(&th_data->thread_mux));
			}

			//close acceptor connection and signal handler
			ERRCHECK(close(poll_array[poll_size - 1].fd));
			ERRCHECK(close(poll_array[poll_size - 2].fd));

			//join threads
			for (size_t i = 0; i < threads_count; i++)
			{
				ERRCHECK(pthread_join(&threads[i], NULL));
			}

			exit = 1;
		}

		if (working_threads < (int)threads_count)
		{
			ERRCHECK(poll(poll_array, poll_size, -1)); //blocking
		}
		else 
		{
			//threads are all working, just listen to signals.
			ERRCHECK(poll(&poll_array[poll_size - 1], 1, -1));
		}

		if (poll_array[poll_size - 1].revents & POLLIN) //check signals
		{
			struct signalfd_siginfo in_signal;
			ERRCHECK(readn(poll_array[poll_size - 1].fd, &in_signal, sizeof(struct signalfd_siginfo)));
			if (in_signal.ssi_signo == SIGINT || in_signal.ssi_signo == SIGQUIT)
			{
				//join all threads, close all connections, exit ---------------
				//set all threads to stop
				for (size_t i = 0; i < threads_count; i++)
				{
					worker_data* th_data = &threads_data[i];
					ERRCHECK(pthread_mutex_lock(&th_data->thread_mux));
					th_data->exit = 1;
					ERRCHECK(pthread_mutex_unlock(&th_data->thread_mux));
				}

				//close acceptor connection and signal handler
				ERRCHECK(close(poll_array[poll_size - 1].fd));
				ERRCHECK(close(poll_array[poll_size - 2].fd));

				//join threads
				for (size_t i = 0; i < threads_count; i++)
				{ ERRCHECK(pthread_join(&threads[i], NULL)); }

				//close all connections
				for (size_t i = 0; i < config_data.max_connections; i++)
				{
					if (poll_array[i].fd != -1)
						ERRCHECK(close(poll_array[i].fd));
				}

				exit = 1;
			}
			else if (in_signal.ssi_signo == SIGHUP)
			{
				//stop incoming connections, terminete when all clients close -
				ERRCHECK(close(poll_array[poll_size - 2].fd)); hup = 1;
			}
			else if (in_signal.ssi_signo == SIGUSR1)
			{
				int work_conn, new_conn;
				//a thread has finished his work, reset the poll call
				for (size_t i = 0; i < threads_count; i++)
				{
					worker_data* th_data = &threads_data[i];
					ERRCHECK(pthread_mutex_lock(&th_data->thread_mux));
					if (th_data->do_work == WORKER_DONE)
					{
						th_data->do_work = WORKER_IDLE;
						work_conn = th_data->work_conn;
						new_conn = th_data->new_conn;
						working_threads--;
					}
					ERRCHECK(pthread_mutex_unlock(&th_data->thread_mux));

					//if work_conn == acceptor, add the new connection
					if (work_conn == sck)
					{
						poll_array[poll_size - 2].fd = work_conn;
						if (new_conn > 0) work_conn = new_conn;
					}

					//add the connection to the poll list
					if (work_conn > 0)
					{
						int done = 0;
						for (size_t i = 0; i < config_data.max_connections && !done; i++)
						{
							if (poll_array[i].fd == -1)
							{
								poll_array[i].fd = work_conn;
								done = 1;
							}
						}
					}

				}
			}
		}
		else if (poll_array[poll_size - 2].revents & POLLIN) //check acceptor
		{
			int fd = poll_array[poll_size - 2].fd;
			poll_array[poll_size - 2].fd = -1;

			int done = 0;
			for (size_t i = 0; i < threads_count && !done; i++)
			{
				worker_data* th_data = &threads_data[i];
				ERRCHECK(pthread_mutex_lock(&th_data->thread_mux));
				if (th_data->do_work == WORKER_IDLE)
				{
					th_data->do_work = WORKER_DO;
					th_data->work_conn = fd;
					th_data->new_conn = fd;
					ERRCHECK(pthread_cond_signal(&th_data->thread_cond));
					working_threads++;
					done = 1;
				}
				ERRCHECK(pthread_mutex_unlock(&th_data->thread_mux));
			}
			if (done == 0) return -1;
		}
		else //check connections
		{
			int done = 0; int fd = -1;
			//find the first connection who needs attention
			for (size_t i = 0; i < config_data.max_connections && !done; i++)
			{
				if (poll_array[i].revents & POLLIN)
				{
					fd = poll_array[i].fd;
					poll_array[i].fd = -1;

					done = 1;
				}
			}

			ERRCHECK(fd);
			done = 0;
			//find the first thread who is idle and assign work
			for (size_t i = 0; i < threads_count && !done; i++)
			{
				worker_data* th_data = &threads_data[i];
				ERRCHECK(pthread_mutex_lock(&th_data->thread_mux));
				if (th_data->do_work == WORKER_IDLE)
				{
					th_data->do_work = WORKER_DO;
					th_data->work_conn = fd;
					th_data->new_conn = -1;
					ERRCHECK(pthread_cond_signal(&th_data->thread_cond));
					working_threads++;
					done = 1;
				}
				ERRCHECK(pthread_mutex_unlock(&th_data->thread_mux));
			}
			if (done == 0) return -1;
		}
	}

	//print stats
	printf("* * * SERVER TERMINATED * * *\n");
	printf("* STATISTICS: *\n  Max Storage: %d Mb;\n  Max Files: %d;"
		"\n  Chace Miss Calls: %d;\n  Files still in storage: %d\n",
		100, 100, 100, 100);
	for (size_t i = 0; i < file_num; i++)
	{
		if (files[i].owner != OWNER_NEXIST)
			printf("  - %s\n", files[i].name);
	}

	//destruction
	destroy_log_file_struct(&log_file);
}

void command_line_parsing(int argc, char* argv[], cfg_t* config_data)
{
	char* cfg = DEFAULT_CONFIG_FILE_NAME;

	int option;
	while ((option = getopt(argc, argv, ":hc:")) != -1)
	{
		switch (option)
		{
		case 'c':
			cfg = optarg;
			break;
		case 'h':
			printf("server application; arguments: [-c config_file] [-h]\n");
			return 0;
		case '?':
		case ':':
			return 1;
		}
	}

	init_default_config(config_data);
	parse_config_from_file(config_data, cfg);
}

void init_default_config(cfg_t* cfg)
{
	if (cfg == NULL) return;
	strncpy(cfg->socket_file, DEFAULT_SOCKET_FILE_NAME, DEFAULT_SOCKET_FILE_SIZE);
	strncpy(cfg->log_file, DEFAULT_LOG_FILE_NAME, DEFAULT_LOG_FILE_SIZE);
	cfg->worker_threads = 4;
	cfg->storage_capacity = 100 * 1024 * 1024; //100 MB
	cfg->max_files = 100;
	cfg->max_connections = 10;
	cfg->algorithm = ALGO_FIFO;
}

void parse_config_from_file(cfg_t* cfg, char const* filename)
{
	cfg_file_t read_cfg;
	ERRCHECKDO(read_config_file(filename, &read_cfg),
		free_config_file(&read_cfg));

	//current valid options in config file:
	//socket_name, log_name, worker_threads, storage_size, max_files, max_connections, algorithm
	char* opt_str; size_t temp; long temp2;
	opt_str = get_option(&read_cfg, "socket_name");
	if (opt_str != NULL && (temp = strlen(opt_str)) < FILENAME_MAX_SIZE)
		strncpy(cfg->socket_file, opt_str, temp + 1);

	opt_str = get_option(&read_cfg, "log_name");
	if (opt_str != NULL && (temp = strlen(opt_str)) < FILENAME_MAX_SIZE)
		strncpy(cfg->log_file, opt_str, temp + 1);

	opt_str = get_option(&read_cfg, "worker_threads");
	if (opt_str != NULL)
	{
		temp2 = strtol(opt_str, NULL, 0);
		if (temp2 > 0 && temp2 < MAX_WORKER_THREADS) cfg->worker_threads = temp2;
	}

	opt_str = get_option(&read_cfg, "storage_size");
	if (opt_str != NULL)
	{
		temp2 = strtol(opt_str, NULL, 0);
		if (temp2 > 0 && temp2 < MAX_STORAGE_CAPACITY) cfg->storage_capacity = temp2;
	}

	opt_str = get_option(&read_cfg, "max_files");
	if (opt_str != NULL)
	{
		temp2 = strtol(opt_str, NULL, 0);
		if (temp2 > 0 && temp2 < MAX_STORAGE_FILES) cfg->max_files = temp2;
	}

	opt_str = get_option(&read_cfg, "max_connections");
	if (opt_str != NULL)
	{
		temp2 = strtol(opt_str, NULL, 0);
		if (temp2 > 0 && temp2 < MAX_CONNECTIONS) cfg->max_connections = temp2;
	}
	
	opt_str = get_option(&read_cfg, "algorithm");
	if (opt_str != NULL)
	{
		if (strncmp(opt_str, "fifo", 4) == 0) cfg->algorithm = ALGO_FIFO;
		else if (strncmp(opt_str, "lru", 3) == 0) cfg->algorithm = ALGO_LRU;
		else if (strncmp(opt_str, "lfu", 3) == 0) cfg->algorithm = ALGO_LFU;
	}
}