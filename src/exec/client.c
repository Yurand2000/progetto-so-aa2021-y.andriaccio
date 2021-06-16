#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "client.h"

static void print_help();

//if it returns 1, the process shall terminate with a success, if returns 0 the process shall
//continue, if -1 the process shall return with error.
static int parse_args(int argc, char* argv[], char** socket_name, int* do_print,
		req_t** reqs, size_t* curr_reqs, size_t* reqs_size);

int main(int argc, char* argv[])
{	
	char* socket_name = NULL;
	req_t* reqs = NULL; size_t curr_reqs = 0, reqs_size = 0;
	int do_print = 0;
        int parse = parse_args(argc, argv, &socket_name, &do_print,
		&reqs, &curr_reqs, &reqs_size);
	if(parse == 1) return 0;
	else if(parse == -1) { perror(""); return 1; }

	//print all requests for testing
	for(size_t i = 0; i < curr_reqs; i++)
	{
		printf("Request %lu, type %d, file %s\n", i, reqs[i].type, reqs[i].filename);
	}

	return 0;
}

static int parse_args(int argc, char* argv[], char** socket_name, int* do_print,
		req_t** reqs, size_t* curr_reqs, size_t* reqs_size)
{
	int option;
	while((option = getopt(argc, argv, "+:hf:w:W:D:r:R::d:t:l:u:c:p")) != -1)
	{
		switch(option)
		{
		case 'h':
			print_help();
			return 1;
		case 'f':
			if(*socket_name != NULL)
			{
				printf("-f flag is specified more than once."
					"Start with -h for details.\n");
				return -1;
			}
			*socket_name = optarg;
			break;
		case 'p':
			if(*do_print == 1)
			{
				printf("-p flag is specified more than once."
					"Start with -h for details.\n");
				return -1;
			}
			else *do_print = 1;
			break;
		case 't':

		//write command
		case 'w':

		case 'W':
		
		case 'D':

		//read command
		case 'r':

		case 'R':

		case 'd':

		//other commands
		case 'l':
			ADD_REQUESTS(REQUEST_LOCK);
			break;
		case 'u':
			ADD_REQUESTS(REQUEST_UNLOCK);
			break;
		case 'c':
			ADD_REQUESTS(REQUEST_CLOSE);
			break;
		//errors
		case ':':
			printf("argument missing error. Start with -h for details.\n");
			return -1;
		case '?':
		default:
			printf("unspecified error!\n");
			return -1;
		}
	}

	return 0;
}

static void print_help()
{
#define HELP_TEXT "FileServer Client, help\n"\
	"Options marked as MANDATORY are mandatory to use (unless calling -h),\n"\
	"while options marked as UNIQUE can be only specified once. If these\n"\
	"condition are not met, the program will terminate.\n"\
	"Options:              | Description:\n"\
	"---- Generic commands: -----------------------------------------\n"\
	"-h                    - Show this help screen, then closes the program.\n"\
	"-f                    - Socketfile to connect to. [MANDATORY, UNIQUE]\n"\
	"-p                    - Print information for each given operation. [UNIQUE]\n"\
	"$-t time               - Time in milliseconds to wait between two requests to the\n"\
	"                        server. If unspecified the time is set to 0.\n"\
	"---- Write file commands: --------------------------------------\n"\
	"-W file1[,file2]      - Send the given file[s] to the file server.\n"\
	"-w dirname[,n=0]      - Send at most n (if n == 0 then all) files from the\n"\
	"                        directory \"dirname\" to the server\n"\
	"-D dirname            - This option goes only with -w or -W. Specifies a folder\n"\
	"                        to write cachemiss files expelled from the server. Can\n"\
	"                        be not specified.\n"\
	"---- Read file commands: ---------------------------------------\n"\
	"-r file1[,file2]      - Read the specified file[s] from the file server. -d option\n"\
	"                        mandatory for this command.\n"\
	"-R [n=0]              - Read any n (if n == 0 then all) from the file server. -d\n"\
	"                        option is mandatory for this command.\n"\
	"-d dirname            - This option goes only with -r or -R. Specifies a folder\n"\
	"                        to write read files from the server. Must be specified for\n"\
	"                        each -r or -R command.\n"\
	"---- File operation commands: ----------------------------------\n"\
	"-l file1[,file2]      - Files to acquire mutual exclusion on its operations.\n"\
	"-u file1[,file2]      - Files to release mutual excluesion on its operations.\n"\
	"-c file1[,file2]      - Files to remove from the server if present.\n"

	printf(HELP_TEXT);
	fflush(stdout);
#undef HELP_TEXT
}
