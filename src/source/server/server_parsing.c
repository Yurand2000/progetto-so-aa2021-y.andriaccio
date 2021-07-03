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

int parse_cmd_start_log(int argc, char* argv[], cfg_t* config_data, log_t* log)
{
	if (command_line_parsing(argc, argv, config_data) == -1 && errno == EINVAL)
		return -1;

	ERRCHECK(init_log_file_struct(log, config_data->log_file));
	ERRCHECK(reset_log(log));
	return 0;
}

int command_line_parsing(int argc, char* argv[], cfg_t* config_data)
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
			return -1;
		case '?':
		case ':':
			errno = EINVAL;
			return -1;
		}
	}

	init_default_config(config_data);
	ERRCHECK(parse_config_from_file(config_data, cfg));
	return 0;
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

int parse_config_from_file(cfg_t* cfg, char const* filename)
{
	cfg_file_t read_cfg;
	ERRCHECK(read_config_file(filename, &read_cfg));

	//current valid options in config file:
	//socket_name, log_name, worker_threads, storage_size, max_files, max_connections, algorithm
	char const* opt_str; size_t temp; long temp2;
	opt_str = get_option(&read_cfg, "socket_name");
	if (opt_str != NULL && (temp = strlen(opt_str)) < FILE_NAME_MAX_SIZE)
		strncpy(cfg->socket_file, opt_str, temp + 1);

	opt_str = get_option(&read_cfg, "log_name");
	if (opt_str != NULL && (temp = strlen(opt_str)) < FILE_NAME_MAX_SIZE)
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

	free_config_file(&read_cfg);
	return 0;
}

int print_stats(shared_state* state, file_t* files, size_t file_num)
{
	printf("* STATISTICS: *\n  Max Storage: %ld bytes (%f Mb);\n  Max Files: %d;"
		"\n  Chace Miss Calls: %d;\n  Files still in storage: %d\n",
		state->max_reached_storage, state->max_reached_storage / (float)(1024*1024),state->max_reached_files,
		state->cache_miss_execs, state->current_files);
	for (size_t i = 0; i < file_num; i++)
	{
		if (files[i].owner != OWNER_NEXIST)
			printf("  - %s\n", files[i].name);
	}
	return 0;
}