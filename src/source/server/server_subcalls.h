#ifndef SERVER_SUBCALLS
#define SERVER_SUBCALLS

#include "../../exec/server.h"
#include "../win32defs.h"

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <poll.h>

#include "../log.h"
#include "../file.h"
#include "../worker.h"
#include "../shared_state.h"

/* all functions are only to be called by the server main function. */
/* all functions return 0 on success, -1 on failure setting errno. */

//server_parsing.c
int parse_cmd_start_log(int argc, char* argv[], cfg_t* config_data, log_t* log);
int command_line_parsing(int argc, char* argv[], cfg_t* config_data);
void init_default_config(cfg_t* cfg);
int parse_config_from_file(cfg_t* cfg, char const* filename);
int print_stats(shared_state* state, file_t* files, size_t file_num);

//server_socket.c
int prepare_socket(int* socket, cfg_t* config);
int signal_to_file(int* signal_fd, sigset_t* oldmask);
int initialize_poll_array(int socket, int signal_fd, struct pollfd** poll_array,
	nfds_t* poll_size, cfg_t* config);

//server_files.c
int initialize_file_structure(file_t** files, size_t* file_num, cfg_t* config);

//server_threads.c
int prepare_shared_data(shared_state* state, int socket, cfg_t* config);
int prepare_threads(pthread_t** threads, size_t* threads_count, worker_data** worker_data,
	file_t* files, size_t file_num,
	shared_state* state, log_t* log, cfg_t* config);

//server_loop.c
int sighup_handler(worker_data* work_data, pthread_t* threads, size_t thread_size,
	struct pollfd* poll_array, nfds_t poll_size, int* exit);
int poll_call(struct pollfd* poll_array, nfds_t poll_size,
	int working_threads, size_t threads_count);

int after_poll_signal(struct pollfd* poll_array, nfds_t poll_size, pthread_t* threads,
	worker_data* threads_data, size_t threads_count, int acceptor,
	int* working_threads, int* exit, int* sighup, cfg_t* config, shared_state* state);
int after_poll_acceptor(struct pollfd* poll_array, nfds_t poll_size, pthread_t* threads,
	worker_data* threads_data, size_t threads_count, int acceptor,
	int* working_threads, int* exit, int* sighup, cfg_t* config);
int after_poll_connection(struct pollfd* poll_array, nfds_t poll_size, pthread_t* threads,
	worker_data* threads_data, size_t threads_count, int acceptor,
	int* working_threads, int* exit, int* sighup, cfg_t* config);

int thread_assign_work(worker_data* threads_data,
	size_t threads_count, int work_conn, int* working_threads);
int sighup_do(struct pollfd* poll_array, nfds_t poll_size, int acceptor, int* sighup, shared_state* state);
int thread_has_finished(struct pollfd* poll_array, nfds_t poll_size,
	worker_data* threads_data, size_t threads_count, int acceptor,
	int* working_threads, cfg_t* config);

//server_subcalls.c
int get_current_connections(shared_state* state, int* curr);

int stop_all_threads(worker_data* threads_data, size_t threads_count);
int join_all_threads(pthread_t* threads, size_t threads_count);
int close_all_client_connections(struct pollfd* poll_array, cfg_t* config);
int add_connection_to_poll(struct pollfd* poll_array, nfds_t poll_size, int conn, cfg_t* config);

#endif