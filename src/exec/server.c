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

int main(int argc, char* argv[])
{
	cfg_t config_data;
	log_t log_file;

	//parse command line and load config file, start logging ------------------
	command_line_parsing(argc, argv, &config_data);
	init_log_file_struct(&log_file, config_data.log_file);
	reset_log(&log_file);

	//prepare socket and start listening --------------------------------------
	size_t len; SOCKNAME_VALID(config_data.socket_file, &len);
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
	ERRCHECK(sigaddset(&sig_mask, SIGUSR1)); //triggered when new connection
	//ignore signals, those are delivered only to the signal file descriptor.
	pthread_sigmask(SIG_SETMASK, &sig_mask, &old_mask);
	int sigfd = signalfd(-1, &sig_mask, 0); //create file descriptor

	//generate poll array
	struct pollfd* poll_array; nfds_t poll_size = config_data.max_connections + 2;
	MALLOC(poll_array, poll_size * sizeof(struct pollfd));

	//fill poll array
	for (size_t i = 0; i < config_data.max_connections; i++)
		poll_array[i].fd = -1;
	poll_array[config_data.max_connections].fd = sck; //listener
	poll_array[config_data.max_connections].events = POLLIN;
	poll_array[config_data.max_connections + 1].fd = sigfd; //signal listener
	poll_array[config_data.max_connections + 1].events = POLLIN;

	//prepare threads
	pthread_t* threads; worker_data* threads_data;
	size_t threads_count = config_data.worker_threads;
	MALLOC(threads, sizeof(pthread_t) * threads_count);
	MALLOC(threads_data, sizeof(worker_data) * threads_count);
	for (size_t i = 0; i < threads_count; i++)
	{
		//fill each worker_data for each thread, then spawn the thread
		ERRCHECKDO( pthread_create(&threads[i], NULL, worker_routine, &threads_data[i]),
			{ free(threads); free(threads_data); } );
	}

	//start threads

	//poll
	poll(, , -1); //blocking

	//if signal, handle it

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
}

void parse_config_from_file(cfg_t* cfg, char const* filename)
{
	cfg_file_t read_cfg;
	ERRCHECKDO(read_config_file(filename, &read_cfg),
		free_config_file(&read_cfg));

	//current valid options in config file:
	//socket_name, log_name, worker_threads, storage_size, max_files, max_connections
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
}