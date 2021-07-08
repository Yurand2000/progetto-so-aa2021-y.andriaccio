#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#include "../source/client/client_subcalls.h"
#include "../source/client_api.h"

int main(int argc, char* argv[])
{
	char* socket_name = NULL;
	char currdir[FILENAME_MAX]; size_t currdir_size;
	req_t* reqs = NULL; size_t curr_reqs = 0, reqs_size = 0;
	int do_print = 0; int time_between_reqs = 0;

	PERRCHECK(get_cwd(currdir, &currdir_size), "CWD Unexpected error");

	//parse command line arguments. -------------------------------------------
	int parse = parse_args(argc, argv, &socket_name, &do_print,
		&reqs, &curr_reqs, &reqs_size, &time_between_reqs);
	if (parse == 1) return 0;
	PERRCHECK(parse == -1, "CMD Parser error");
	PERRCHECK(check_socket_file(socket_name) == -1, "Socket File Error");

	//if there are no requests, print the help
	if (curr_reqs == 0) { print_help(); return 0; }

	PERRCHECK(expand_requests(&reqs, &curr_reqs, &reqs_size, currdir, currdir_size) == -1,
		"Expand Requests Parse Error");
	PERRCHECK(add_open_create_requests(&reqs, &curr_reqs, &reqs_size) == -1,
		"Open/Create Parser Error");

	//call the client api -----------------------------------------------------
	PERRCHECK(open_connection(socket_name, time_between_reqs) == -1, "Couldn't connect to server");
	PERRCHECK(send_requests(reqs, curr_reqs, time_between_reqs, do_print) == -1, "Request failure");
	closeConnection(socket_name);

	//clean and ---------------------------------------------------------------
	for (size_t i = 0; i < curr_reqs; i++)
		destroy_request(&reqs[i]);
	free(reqs);

	return 0;
}