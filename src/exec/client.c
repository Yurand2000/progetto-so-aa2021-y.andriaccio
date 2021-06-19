#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#include "../source/client_api.h"

static void print_help();

//if it returns 1, the process shall terminate with a success, if returns 0 the process shall
//continue, if -1 the process shall return with error.
static int parse_args(int argc, char* argv[], char** socket_name, int* do_print,
		req_t** reqs, size_t* curr_reqs, size_t* reqs_size, int* time_between_reqs);

//expands the given directory to its files in requests.
static int expand_dir_to_files(char* dirname, int max, req_t** reqs,
	size_t* curr_reqs, size_t* reqs_size, int* count_ptr);

int main(int argc, char* argv[])
{
	char* socket_name = NULL;

	req_t* reqs = NULL; size_t curr_reqs = 0, reqs_size = 0;
	int do_print = 0; int time_between_reqs = 0;

	//parse command line arguments. -------------------------------------------
	int parse = parse_args(argc, argv, &socket_name, &do_print,
		&reqs, &curr_reqs, &reqs_size, &time_between_reqs);
	if (parse == 1) return 0;
	else if (parse == -1) { perror(""); return 1; }

	if (socket_name == NULL) { printf("-f flag is mandatory. start with -h for details.\n"); return 0; }
	else printf("socket file: %s\n", socket_name);
	if (curr_reqs == 0) { print_help(); return 0; }

	//print all requests for testing
	for(size_t i = 0; i < curr_reqs; i++)
	{
		printf("Request: %lu; type: %d; n: %d;", i, reqs[i].type, reqs[i].n);
		if (reqs[i].stringdata != NULL) printf(" files: %s;", reqs[i].stringdata);
		if (reqs[i].dir != NULL) printf(" dir: %s;", reqs[i].dir);
		printf("\n");
	}

	printf("\n\n-----------------------------------------------------------\n");
	//expand all requests that have a directory instead of a file. ------------
	req_t* reqs_exp = NULL; size_t curr_reqs_exp = 0, reqs_size_exp = 0;
	for (size_t i = 0; i < curr_reqs; i++)
	{
		if (reqs[i].type == REQUEST_WRITE_DIR)
		{
			expand_dir_to_files(reqs[i].stringdata, reqs[i].n,
				&reqs_exp, &curr_reqs_exp, &reqs_size_exp, NULL);
			destroy_request(&reqs[i]);
		}
		else
		{
			add_request(reqs[i], &reqs_exp, &curr_reqs_exp, &reqs_size_exp);
		}
	}
	
	//print all requests for testing
	for (size_t i = 0; i < curr_reqs; i++)
	{
		printf("Request: %lu; type: %d; n: %d;", i, reqs[i].type, reqs[i].n);
		if (reqs[i].stringdata != NULL) printf(" files: %s;", reqs[i].stringdata);
		if (reqs[i].dir != NULL) printf(" dir: %s;", reqs[i].dir);
		printf("\n");
	}

	printf("\n\n-----------------------------------------------------------\n");
	//add open/create and close requests. -------------------------------------
	curr_reqs = 0;	//using the old reqs array, but as if it was empty.
					//all old memory has already been cleared.
#ifdef OPEN_CREATE_REQS
	free(reqs_exp);

	//print all requests for testing
	for (size_t i = 0; i < curr_reqs; i++)
	{
		printf("Request: %lu; type: %d; n: %d;", i, reqs[i].type, reqs[i].n);
		if (reqs[i].stringdata != NULL) printf(" files: %s;", reqs[i].stringdata);
		if (reqs[i].dir != NULL) printf(" dir: %s;", reqs[i].dir);
		printf("\n");
	}
	printf("\n\n-----------------------------------------------------------\n");
#endif
	//call the client api -----------------------------------------------------
#ifdef CLIENT_API_ENABLE
#define REQ_FAIL(res) if (res == -1) { perror("Request failure: "); }

	int res; FILE* fd;
	void* buf = NULL; size_t size = 0;

	//connect
	struct timespec abstime; abstime.tv_sec = 10; abstime.tv_nsec = 0;
	struct timespec waittime; waittime.tv_sec = 0; waittime.tv_nsec = time_between_reqs * 1000;
	int res = openConnection(socket_name, time_between_reqs, abstime);
	if(res == -1)
	{
		printf("couldn't connect to server!\n");
		return -1;
	}

	//pass requests
	for (size_t i = 0; i < curr_reqs; i++)
	{
		switch (reqs[i].type)
		{
		case REQUEST_OPEN:
			res = openFile(reqs[i].stringdata, 0);
			REQ_FAIL(res);
			break;
		case REQUEST_CREATE_LOCK:
			res = openFile(reqs[i].stringdata, O_CREATE | O_LOCK);
			REQ_FAIL(res);
			break;
		case REQUEST_CLOSE:
			res = closeFile(reqs[i].stringdata);
			REQ_FAIL(res);
			break;
		case REQUEST_READ:
			res = readFile(reqs[i].stringdata, &buf, &size);
			if (res == -1) { perror("Request failure: "); }
			else if (reqs[i].dir != NULL)
			{
				//create file and write buf data
				PTRCHECK( (fd = fopen(reqs[i].stringdata, "wb")) );
				fwrite(buf, 1, size, fd);
				fclose(fd);
			}
			free(buf);
			free(size);
			break;
		case REQUEST_READN:
			res = readNFiles(reqs[i].n, reqs[i].dir);
			REQ_FAIL(res);
			break;
		case REQUEST_WRITE:
			res = writeFile(reqs[i].stringdata, reqs[i].dir);
			REQ_FAIL(res);
			break;
		case REQUEST_LOCK:
			res = lockFile(reqs[i].stringdata);
			REQ_FAIL(res);
			break;
		case REQUEST_UNLOCK:
			res = unlockFile(reqs[i].stringdata);
			REQ_FAIL(res);
			break;
		case REQUEST_REMOVE:
			res = removeFile(reqs[i].stringdata);
			REQ_FAIL(res);
			break;
		default:
			break;
		}

		//wait
		nanosleep(&waittime, NULL);
	}

	closeConnection(socket_name);
#endif

	//clean and close
	for (size_t i = 0; i < curr_reqs; i++)
		destroy_request(&reqs[i]);
	free(reqs);

	return 0;
}

static int parse_args(int argc, char* argv[], char** socket_name, int* do_print,
		req_t** reqs, size_t* curr_reqs, size_t* reqs_size, int* time_between_reqs)
{
	int option, prec;
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
				printf("-f flag is specified more than once. "
					"Start with -h for details.\n");
				return -1;
			}
			*socket_name = optarg;
			break;
		case 'p':
			if(*do_print == 1)
			{
				printf("-p flag is specified more than once. "
					"Start with -h for details.\n");
				return -1;
			}
			else *do_print = 1;
			break;
		case 't':
			*time_between_reqs = (int)strtol(optarg, NULL, 0);
			break;
		//write command
		case 'w':
			CMD_TO_REQUEST(REQUEST_WRITE_DIR);
			break;
		case 'W':
			CMD_TO_REQUEST(REQUEST_WRITE);
			break;
		case 'D':
			if (prec != 'w' && prec != 'W')
			{
				printf("-D flag requires a preceding -w or -W flag. "
					"Start with -h for details.\n");
				return -1;
			}
			else
				WRITE_DIR_TO_LAST_REQUEST
			break;
		//read command
		case 'r':
			CMD_TO_REQUEST(REQUEST_READ);
			break;
		case 'R':
			CMD_TO_REQUEST(REQUEST_READN);
			break;
		case 'd':
			if (prec != 'r' && prec != 'R')
			{
				printf("-d flag requires a preceding -r or -R flag. "
					"Start with -h for details.\n");
				return -1;
			}
			else
				WRITE_DIR_TO_LAST_REQUEST
			break;
		//other commands
		case 'l':
			CMD_TO_REQUEST(REQUEST_LOCK);
			break;
		case 'u':
			CMD_TO_REQUEST(REQUEST_UNLOCK);
			break;
		case 'c':
			CMD_TO_REQUEST(REQUEST_REMOVE);
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
		prec = option;
	}

	return 0;
}

static void print_help()
{
#define HELP_TEXT "FileServer Client, help:\n"\
"Options marked as MANDATORY are mandatory to use (unless calling -h) while\n"\
"options marked as UNIQUE can be only specified once. If these conditions are\n"\
"not met, the program will terminate.\n"\
"Options:              | Description:\n"\
"---- Generic commands: -----------------------------------------\n"\
"-h                    - Show this help screen, then closes the program.\n"\
"-f                    - Socketfile to connect to. [MANDATORY, UNIQUE]\n"\
"-p                    - Print information for each given operation. [UNIQUE]\n"\
"-t time               - Time in milliseconds to wait between two requests to\n"\
"                        the server. If unspecified the time is set to 0.\n"\
"---- Write file commands: --------------------------------------\n"\
"-W file1[,file2]      - Send the given file[s] to the file server.\n"\
"-w dirname[,n=0]      - Send at most n (if n == 0 then all) files from the\n"\
"                        directory \"dirname\" to the server\n"\
"[-D dirname]          - This option goes only with -w or -W. Specifies a folder\n"\
"                        to write cachemiss files expelled from the server.\n"\
"                        Can be not specified.\n"\
"---- Read file commands: ---------------------------------------\n"\
"-r file1[,file2]      - Read the specified file[s] from the file server.\n"\
"                        -d option mandatory for this command.\n"\
"-R [n=0]              - Read any n (if n == 0 then all) from the file server.\n"\
"                        -d option is mandatory for this command.\n"\
"-d dirname            - This option goes only with -r or -R. Specifies a folder\n"\
"                        to save read files. Can be not specified.\n"\
"---- File operation commands: ----------------------------------\n"\
"-l file1[,file2]      - Files to acquire mutual exclusion on its operations.\n"\
"-u file1[,file2]      - Files to release mutual exclusion on its operations.\n"\
"-c file1[,file2]      - Files to remove from the server if present.\n"

	printf(HELP_TEXT);
	fflush(stdout);
#undef HELP_TEXT
}

int expand_dir_to_files(char* dirname, int max, req_t** reqs, size_t* curr_reqs,
	size_t* reqs_size, int* count_ptr)
{
	int count = 0;
	if (count_ptr == NULL) count_ptr = &count;

	struct dirent* entry;
	DIR* dir_ptr; req_t temp;
	temp.type = REQUEST_WRITE;
	init_request(&temp);
	PTRCHECK((dir_ptr = opendir(dirname)));
	do
	{
		entry = readdir(dir_ptr);
		if (entry->d_type == DT_REG) {
			temp.stringdata_len = strlen(entry->d_name);
			MALLOC(temp.stringdata, temp.stringdata_len);
			strncpy(temp.stringdata, entry->d_name, temp.stringdata_len);
			add_request(temp, reqs, curr_reqs, reqs_size);
			(*count_ptr)++;
		}
		else if (entry->d_type == DT_DIR)
			expand_dir_to_files(entry->d_name, max, reqs, curr_reqs, reqs_size, count_ptr);
	} while (entry != NULL && (max == 0 || *count_ptr < max));
	ERRCHECK(closedir(dir_ptr));
	return 0;
}