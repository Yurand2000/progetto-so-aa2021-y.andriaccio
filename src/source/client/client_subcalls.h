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

//expands the given directory to its files in requests.
int expand_dir_to_files(char* dirname, int max, char* retdir, size_t retdir_size,
	req_t** reqs, size_t* curr_reqs, size_t* reqs_size,
	int* count_ptr, const char* currdir, size_t currdir_size);

//split multifile requests and changes local to absolute path
int split_and_fix_request_files(req_t* req, req_t** reqs,
	size_t* curr_reqs, size_t* reqs_size, const char* currdir, size_t currdir_size);

//add open/create and close request after each operation
int add_open_create_requests(req_t* reqs, size_t reqs_num, req_t** out_reqs,
	size_t* out_curr_reqs, size_t* out_reqs_size);

void print_help();

//-p option printer
void print_operation_result(const char* op_type, const char* file, int res);

#endif
