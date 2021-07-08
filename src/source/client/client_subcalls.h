#ifndef CLIENT_SUBCALLS
#define CLIENT_SUBCALLS
#include "../win32defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

#include "../errset.h"
#include "../request.h"

//if it returns 1, the process shall terminate with a success, if returns 0 the process shall
//continue, if -1 the process shall return with error.
int parse_args(int argc, char* argv[], char** socket_name, int* do_print,
	req_t** reqs, size_t* curr_reqs, size_t* reqs_size, int* time_between_reqs);

//expands the requests having a directory as argument and multifile requests.
int expand_requests(req_t** reqs, size_t* curr_reqs, size_t* reqs_size,
	char currdir[], size_t currdir_size);

//add open/create and close request after each operation
int add_open_create_requests(req_t** out_reqs, size_t* out_curr_reqs, size_t* out_reqs_size);

//-p option printer
void print_operation_result(const char* op_type, const char* file, int res);

int open_connection(char* socket_name, int time_between_reqs);
int send_requests(req_t* reqs, size_t curr_reqs, int time_between_reqs, int do_print);

void print_help();
int get_cwd(char currdir[], size_t* currdir_size);
int check_socket_file(char* socket_name);

#endif
