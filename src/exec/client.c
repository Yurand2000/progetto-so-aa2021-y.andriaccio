#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#define _DEBUG
#define CLIENT_API_ENABLE

#include "../source/client_api.h"

static void print_help();

//if it returns 1, the process shall terminate with a success, if returns 0 the process shall
//continue, if -1 the process shall return with error.
static int parse_args(int argc, char* argv[], char** socket_name, int* do_print,
		req_t** reqs, size_t* curr_reqs, size_t* reqs_size, int* time_between_reqs);

static int cmd_to_request(int type, req_t** reqs, size_t* curr_reqs, size_t* reqs_size);
static int write_dir_to_last_request(req_t** reqs, size_t* curr_reqs);

//expands the given directory to its files in requests.
static int expand_dir_to_files(char* dirname, int max, char* retdir, size_t retdir_size,
	req_t** reqs, size_t* curr_reqs, size_t* reqs_size,
	int* count_ptr, const char* currdir, size_t currdir_size);

//split multifile requests and changes local to absolute path
static int split_and_fix_request_files(req_t* req, req_t** reqs,
	size_t* curr_reqs, size_t* reqs_size, const char* currdir, size_t currdir_size);

//add open/create and close request after each operation
static int add_open_create_requests(req_t* reqs, size_t reqs_num, req_t** out_reqs,
	size_t* out_curr_reqs, size_t* out_reqs_size);

static void print_operation_result(const char* op_type, const char* file, int res);

int main(int argc, char* argv[])
{
	char* socket_name = NULL;
	char currdir[FILENAME_MAX]; size_t currdir_size;
	PERRCHECK(getcwd(currdir, FILENAME_MAX - 1) == NULL, "CWD Unexpected error");
	currdir_size = strlen(currdir);

	//add trailing / if necessary
	if (currdir[currdir_size - 1] != '/')
	{
		currdir[currdir_size] = '/';
		currdir[currdir_size + 1] = '\0';
		currdir_size++;
	}

	req_t* reqs = NULL; size_t curr_reqs = 0, reqs_size = 0;
	int do_print = 0; int time_between_reqs = 0;

	//parse command line arguments. -------------------------------------------
	int parse = parse_args(argc, argv, &socket_name, &do_print,
		&reqs, &curr_reqs, &reqs_size, &time_between_reqs);
	if (parse == 1) return 0;
	else if (parse == -1)
	{
		if(errno != 0)
			perror("Error");
		return -1;
	}

	if (socket_name == NULL || socket_name[0] == '\0')
	{
		printf("-f flag is mandatory. start with -h for details.\n");
		return -1;
	}
	else
	{
		ERRCHECKDO(access(socket_name, R_OK | W_OK),
			printf("socket file does not exist. Has the server started?\n"));
	}

	if (curr_reqs == 0) { print_help(); return 0; }

#ifdef _DEBUG
	//print all requests for testing
	for (size_t i = 0; i < curr_reqs; i++)
	{
		printf("Request: %3lu; type: %4d; n: %5d;", i, reqs[i].type, reqs[i].n);
		if (reqs[i].stringdata != NULL) printf(" files: %s;", reqs[i].stringdata);
		if (reqs[i].dir != NULL) printf(" dir: %s;", reqs[i].dir);
		printf("\n");
	}

	printf("\n\n-----------------------------------------------------------\n");
#endif
	//expand all requests that have a directory instead of a file. ------------
	req_t* reqs_exp = NULL; size_t curr_reqs_exp = 0, reqs_size_exp = 0;
	for (size_t i = 0; i < curr_reqs; i++)
	{
		if (reqs[i].type == REQUEST_WRITE_DIR)
		{
			expand_dir_to_files(reqs[i].stringdata, reqs[i].n,
				reqs[i].dir, reqs[i].dir_len,
				&reqs_exp, &curr_reqs_exp, &reqs_size_exp,
				NULL, currdir, currdir_size);
			destroy_request(&reqs[i]);
		}
		else if (reqs[i].type == REQUEST_READN)
		{
			add_request(reqs[i], &reqs_exp, &curr_reqs_exp, &reqs_size_exp);
		}
		else
		{
			split_and_fix_request_files(&reqs[i], &reqs_exp, &curr_reqs_exp,
				&reqs_size_exp, currdir, currdir_size);
			destroy_request(&reqs[i]);
		}
	}

#ifdef _DEBUG
	//print all requests for testing
	for (size_t i = 0; i < curr_reqs_exp; i++)
	{
		printf("Request: %3lu; type: %4d; n: %5d;", i, reqs_exp[i].type, reqs_exp[i].n);
		if (reqs_exp[i].stringdata != NULL) printf(" files: %s;", reqs_exp[i].stringdata);
		if (reqs_exp[i].dir != NULL) printf(" dir: %s;", reqs_exp[i].dir);
		printf("\n");
	}
	printf("\n\n-----------------------------------------------------------\n");

#endif
	//add open/create and close requests. -------------------------------------
	curr_reqs = 0;	//using the old reqs array, but as if it was empty.
					//all old memory has already been cleared.

	add_open_create_requests(reqs_exp, curr_reqs_exp, &reqs, &curr_reqs, &reqs_size);
	free(reqs_exp);

#ifdef _DEBUG
	//print all requests for testing
	for (size_t i = 0; i < curr_reqs; i++)
	{
		printf("Request: %3lu; type: %4d; n: %5d;", i, reqs[i].type, reqs[i].n);
		if (reqs[i].stringdata != NULL) printf(" files: %s;", reqs[i].stringdata);
		if (reqs[i].dir != NULL) printf(" dir: %s;", reqs[i].dir);
		printf("\n");
	}
	printf("\n\n-----------------------------------------------------------\n");
#endif

	//call the client api -----------------------------------------------------
#ifdef CLIENT_API_ENABLE
#define DO_PRINT(op, file, res) if (do_print) print_operation_result(op, file, res);

	int res; FILE* fd;
	void* buf = NULL; size_t size = 0;

	//connect
	struct timespec abstime; abstime.tv_sec = 10; abstime.tv_nsec = 0;
	long nsec = (long)time_between_reqs * 1000000L;
	struct timespec waittime; waittime.tv_sec = nsec / 1000000000L; waittime.tv_nsec = nsec % 1000000000L;
	res = openConnection(socket_name, time_between_reqs, abstime);
	if(res == -1)
	{
		printf("couldn't connect to server!\n");
		return -1;
	}

	//pass requests
	for (size_t i = 0; i < curr_reqs; i++)
	{
		//wait
		nanosleep(&waittime, NULL);

		switch (reqs[i].type)
		{
		case REQUEST_OPEN:
			res = openFile(reqs[i].stringdata, 0);
			DO_PRINT("Open File", reqs[i].stringdata, res);
			break;
		case REQUEST_OPEN_LOCK:
			res = openFile(reqs[i].stringdata, O_LOCK);
			DO_PRINT("Open Lock File", reqs[i].stringdata, res);
			break;
		case REQUEST_CREATE_LOCK:
			res = openFile(reqs[i].stringdata, O_CREATE | O_LOCK);
			DO_PRINT("Create and Lock File", reqs[i].stringdata, res);
			break;
		case REQUEST_CLOSE:
			res = closeFile(reqs[i].stringdata);
			DO_PRINT("Close File", reqs[i].stringdata, res);
			break;
		case REQUEST_READ:
			res = readFile(reqs[i].stringdata, &buf, &size);
			DO_PRINT("Read File", reqs[i].stringdata, res);
			if (res != -1 && reqs[i].dir != NULL)
			{
				//create file and write buf data
				size_t dirlen = strlen(reqs[i].dir);
				size_t len = strlen(reqs[i].stringdata) + 1;
				char* file = malloc(len + dirlen);
				if (file != NULL)
				{
					strncpy(file, reqs[i].dir, dirlen);
					strncpy(file + dirlen, reqs[i].stringdata, len);
					fd = fopen(file, "wb");
					if (fd != NULL)
					{
						fwrite(buf, sizeof(char), size, fd);
						fclose(fd);
					}
					else perror("Write to file failure");
					free(file);
				}
				else perror("Write to file failure");
			}
			free(buf);
			break;
		case REQUEST_READN:
			res = readNFiles(reqs[i].n, reqs[i].dir);
			DO_PRINT("Read N Files", reqs[i].stringdata, res);
			break;
		case REQUEST_WRITE:
			res = writeFile(reqs[i].stringdata, reqs[i].dir);
			DO_PRINT("Write File", reqs[i].stringdata, res);
			break;
		case REQUEST_LOCK:
			res = lockFile(reqs[i].stringdata);
			DO_PRINT("Lock File", reqs[i].stringdata, res);
			break;
		case REQUEST_UNLOCK:
			res = unlockFile(reqs[i].stringdata);
			DO_PRINT("Unlock File", reqs[i].stringdata, res);
			break;
		case REQUEST_REMOVE:
			res = removeFile(reqs[i].stringdata);
			DO_PRINT("Remove File", reqs[i].stringdata, res);
			break;
		default:
			break;
		}
	}
	//wait
	nanosleep(&waittime, NULL);

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
	while((option = getopt(argc, argv, "-:hf:w:W:D:r:R::d:t:l:u:c:p")) != -1)
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
			if (*time_between_reqs < 0)
			{
				printf("Time between requests can't be negative. "
					"Sorry, we can't go back in time yet... "
					"Start with -h for details.\n");
				return -1;
			}
			break;
		//write command
		case 'w':
			ERRCHECK(cmd_to_request(REQUEST_WRITE_DIR, reqs, curr_reqs, reqs_size));
			break;
		case 'W':
			ERRCHECK(cmd_to_request(REQUEST_WRITE, reqs, curr_reqs, reqs_size));
			break;
		case 'D':
			if (prec != 'w' && prec != 'W')
			{
				printf("-D flag requires a preceding -w or -W flag. "
					"Start with -h for details.\n");
				return -1;
			}
			else
				ERRCHECK(write_dir_to_last_request(reqs, curr_reqs));
			break;
		//read command
		case 'r':
			ERRCHECK(cmd_to_request(REQUEST_READ, reqs, curr_reqs, reqs_size));
			break;
		case 'R':
			ERRCHECK(cmd_to_request(REQUEST_READN, reqs, curr_reqs, reqs_size));
			break;
		case 'd':
			if (prec != 'r' && prec != 'R')
			{
				printf("-d flag requires a preceding -r or -R flag. "
					"Start with -h for details.\n");
				return -1;
			}
			else
				ERRCHECK(write_dir_to_last_request(reqs, curr_reqs));
			break;
		//other commands
		case 'l':
			ERRCHECK(cmd_to_request(REQUEST_LOCK, reqs, curr_reqs, reqs_size));
			break;
		case 'u':
			ERRCHECK(cmd_to_request(REQUEST_UNLOCK, reqs, curr_reqs, reqs_size));
			break;
		case 'c':
			ERRCHECK(cmd_to_request(REQUEST_REMOVE, reqs, curr_reqs, reqs_size));
			break;
		//errors
		case ':':
			printf("Argument missing error. Start with -h for details.\n");
			return -1;
		case '?':
		default:
			printf("Unspecified error!\n");
			return -1;
		case 1:
			printf("The given options are not formatted correctly. Start with -h for details.\n");
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
"                        It is optional.\n"\
"---- Read file commands: ---------------------------------------\n"\
"-r file1[,file2]      - Read the specified file[s] from the file server.\n"\
"                        -d option optional for this command.\n"\
"-R[n=0]               - Read any n (if n == 0 then all) from the file server.\n"\
"                        -d option is optional for this command.\n"\
"                        Note that the optional argument must have no spaces\n"\
"                        with the -R option, otherwise it will be skipped.\n"\
"-d dirname            - This option goes only with -r or -R. Specifies a folder\n"\
"                        to save read files. It is optional.\n"\
"---- File operation commands: ----------------------------------\n"\
"-l file1[,file2]      - Files to acquire mutual exclusion on its operations.\n"\
"-u file1[,file2]      - Files to release mutual exclusion on its operations.\n"\
"-c file1[,file2]      - Files to remove from the server if present.\n"\
"                        The operation requires mutual exclusion on the files.\n\n"

	printf(HELP_TEXT);
	fflush(stdout);
#undef HELP_TEXT
}

static int expand_dir_to_files(char* dirname, int max, char* retdir, size_t retdir_size,
	req_t** reqs, size_t* curr_reqs, size_t* reqs_size,
	int* count_ptr, const char* currdir, size_t currdir_size)
{
	if (dirname == NULL) ERRSET(EINVAL, -1);

	//first call initializes the counter to compare against max ---------------
	int count = 0;
	if (count_ptr == NULL) count_ptr = &count;

	//generate the absolute path string ---------------------------------------
	char* abspath; size_t len, abspath_size;
	
	abspath_size = len = strlen(dirname);
	if (dirname[0] == '/')
	{
		MALLOC(abspath, sizeof(char) * abspath_size + 1);
		strncpy(abspath, dirname, abspath_size);
	}
	else
	{
		abspath_size += currdir_size;
		MALLOC(abspath, sizeof(char) * abspath_size + 1);
		strncpy(abspath, currdir, currdir_size);
		strncpy(abspath + currdir_size, dirname, len);
		abspath[abspath_size] = '\0';
	}

	//add trailing slash if necessary
	if (abspath[abspath_size - 1] != '/')
	{
		abspath_size++;
		REALLOC(abspath, abspath, abspath_size + 1);
		abspath[abspath_size - 1] = '/';
		abspath[abspath_size] = '\0';
	}

	//open the directory and look for files -----------------------------------
	struct dirent* entry;
	DIR* dir_ptr; req_t temp;

	init_request(&temp);
	temp.type = REQUEST_WRITE;
	temp.dir_len = retdir_size;

	PTRCHECK((dir_ptr = opendir(abspath)));

	entry = readdir(dir_ptr);
	while (entry != NULL && (max == 0 || *count_ptr < max))
	{
		if (entry->d_type == DT_REG)
		{
			temp.stringdata_len = len = strlen(entry->d_name) + 1;
			temp.stringdata_len += abspath_size;
			MALLOC(temp.stringdata, sizeof(char) * temp.stringdata_len);
			strncpy(temp.stringdata, abspath, abspath_size);
			strncpy(temp.stringdata + abspath_size, entry->d_name, len);

			if (temp.dir_len != 0)
			{
				MALLOC(temp.dir, sizeof(char) * temp.dir_len);
				strncpy(temp.dir, retdir, temp.dir_len);
			}

			add_request(temp, reqs, curr_reqs, reqs_size);
			(*count_ptr)++;
		}
		else if (entry->d_type == DT_DIR && strncmp(entry->d_name, ".", 2) != 0
			&& strncmp(entry->d_name, "..", 3) != 0)
		{
			expand_dir_to_files(entry->d_name, max, retdir, retdir_size,
				reqs, curr_reqs, reqs_size,
				count_ptr, abspath, abspath_size);
		}
		entry = readdir(dir_ptr);
	}
	free(abspath);

	ERRCHECK(closedir(dir_ptr));
	return 0;
}

static int split_and_fix_request_files(req_t* req, req_t** reqs,
	size_t* curr_reqs, size_t* reqs_size, const char* currdir, size_t currdir_size)
{
	char* saveptr; char* token;
	req_t new_req; size_t len;
	init_request(&new_req);
	new_req.type = req->type;
	new_req.dir_len = req->dir_len;

	token = strtok_r(req->stringdata, ",", &saveptr);
	while (token != NULL)
	{
		new_req.stringdata_len = len = strlen(token) + 1;
		if (token[0] != '/') new_req.stringdata_len += currdir_size;
		MALLOC(new_req.stringdata, sizeof(char) * new_req.stringdata_len);
		if (token[0] != '/')
		{
			strncpy(new_req.stringdata, currdir, currdir_size);
			strncpy(new_req.stringdata + currdir_size, token, len);
		}
		else
			strncpy(new_req.stringdata, token, new_req.stringdata_len);

		if (new_req.dir_len != 0)
		{
			MALLOC(new_req.dir, sizeof(char) * new_req.dir_len);
			strncpy(new_req.dir, req->dir, new_req.dir_len);
		}

		add_request(new_req, reqs, curr_reqs, reqs_size);
		token = strtok_r(NULL, ",", &saveptr);
	}

	return 0;
}

static int add_open_create_requests(req_t* reqs, size_t reqs_num, req_t** out_reqs,
	size_t* out_curr_reqs, size_t* out_reqs_size)
{
	req_t temp;
	init_request(&temp);

	for (size_t i = 0; i < reqs_num; i++)
	{
		req_t* req = &reqs[i];

		//insert the open request
		if (req->type == REQUEST_READ || req->type == REQUEST_LOCK || req->type == REQUEST_UNLOCK)
		{
			int insert = 0;
			for (int j = *out_curr_reqs - 1; j >= 0 && insert == 0; j--)
			{
				if ((*out_reqs)[j].type == REQUEST_REMOVE) insert = 1;
				if (  ( (*out_reqs)[j].type == REQUEST_OPEN || (*out_reqs)[j].type == REQUEST_OPEN_LOCK
						|| (*out_reqs)[j].type == REQUEST_CREATE_LOCK )
					&& strcmp((*out_reqs)[j].stringdata, req->stringdata) == 0  ) insert = -1;
			}

			if (insert != -1)
			{
				temp.type = REQUEST_OPEN;
				temp.stringdata_len = req->stringdata_len;
				MALLOC(temp.stringdata, sizeof(char) * temp.stringdata_len);
				strncpy(temp.stringdata, req->stringdata, temp.stringdata_len);

				ERRCHECK(add_request(temp, out_reqs, out_curr_reqs, out_reqs_size));
			}
		}

		//specific requests for write and remove file
		if (reqs[i].type == REQUEST_WRITE)
		{
			temp.type = REQUEST_CREATE_LOCK;
			temp.stringdata_len = reqs[i].stringdata_len;
			MALLOC(temp.stringdata, sizeof(char) * temp.stringdata_len);
			strncpy(temp.stringdata, reqs[i].stringdata, temp.stringdata_len);

			ERRCHECK(add_request(temp, out_reqs, out_curr_reqs, out_reqs_size));
		}
		else if (reqs[i].type == REQUEST_REMOVE)
		{
			temp.type = REQUEST_OPEN_LOCK;
			temp.stringdata_len = reqs[i].stringdata_len;
			MALLOC(temp.stringdata, sizeof(char) * temp.stringdata_len);
			strncpy(temp.stringdata, reqs[i].stringdata, temp.stringdata_len);

			ERRCHECK(add_request(temp, out_reqs, out_curr_reqs, out_reqs_size));
		}

		//push the actual request
		ERRCHECK(add_request(reqs[i], out_reqs, out_curr_reqs, out_reqs_size))
		
		//insert the close request
		if (req->type == REQUEST_READ || req->type == REQUEST_LOCK || req->type == REQUEST_UNLOCK
			|| req->type == REQUEST_WRITE)
		{
			int insert = 0;
			for (int j = i + 1; j < reqs_num; j++)
			{
				if(strcmp(reqs[j].stringdata, req->stringdata) == 0) insert = 1;
			}
			for (int j = *out_curr_reqs - 1; j >= 0 && insert == 0; j--)
			{
				if ((*out_reqs)[j].type == REQUEST_REMOVE) insert = 1;
				if (  ( (*out_reqs)[j].type == REQUEST_OPEN || (*out_reqs)[j].type == REQUEST_OPEN_LOCK
					|| (*out_reqs)[j].type == REQUEST_CREATE_LOCK )
					&& strcmp((*out_reqs)[j].stringdata, req->stringdata) == 0  ) insert = -1;
			}

			if (insert == -1)
			{
				temp.type = REQUEST_CLOSE;
				temp.stringdata_len = req->stringdata_len;
				MALLOC(temp.stringdata, sizeof(char) * temp.stringdata_len);
				strncpy(temp.stringdata, req->stringdata, temp.stringdata_len);

				ERRCHECK(add_request(temp, out_reqs, out_curr_reqs, out_reqs_size));
			}
		}
	}
	return 0;
}

static void print_operation_result(const char* op_type, const char* file, int res)
{
	char* op_res = (res != -1) ? "Success" : strerror(errno);

	int bytes_read, bytes_write;
	get_bytes_read_write(&bytes_read, &bytes_write);
	printf("Operation: %s; File: %s; Result: %s", op_type, file, op_res);
	if(res > 0)
		printf("; Files read: %d", res);
	if(bytes_read != 0 || bytes_write != 0)
		printf("; Bytes: read %d; write %d", bytes_read, bytes_write);
	printf(".\n");
}

static int write_dir_to_last_request(req_t** reqs, size_t* curr_reqs)
{
	req_t* last = &(*reqs)[*curr_reqs - 1];
	size_t len = strlen(optarg);
	last->dir_len = len + 1;
	if (optarg[last->dir_len - 2] != '/')
		last->dir_len++;

	MALLOC(last->dir, last->dir_len);

	strncpy(last->dir, optarg, len + 1);
	if (optarg[last->dir_len - 2] != '/')
	{
		last->dir[last->dir_len - 2] = '/';
		last->dir[last->dir_len - 1] = '\0';
	}
	return 0;
}

static int cmd_to_request(int type, req_t** reqs, size_t* curr_reqs, size_t* reqs_size)
{
	req_t request;
	init_request(&request);
	
	request.type = type;
	if(type == REQUEST_READN)
	{
		if(optarg != NULL)
			request.n = strtol(optarg, NULL, 0);
	}
	else if (type == REQUEST_WRITE_DIR)
	{
		char* saveptr = NULL;
		char* token = strtok_r(optarg, ",", &saveptr);
		request.stringdata_len = strlen(token) + 1;
		MALLOC(request.stringdata, request.stringdata_len);
		strncpy(request.stringdata, token, request.stringdata_len);
		
		token = strtok_r(NULL, ",", &saveptr);
		if(token != NULL) request.n = strtol(token, NULL, 0);
	}
	else
	{
		request.stringdata_len = strlen(optarg) + 1;
		MALLOC(request.stringdata, request.stringdata_len);
		strncpy(request.stringdata, optarg, request.stringdata_len);
	}
	
	ERRCHECK(add_request(request, reqs, curr_reqs, reqs_size));
	return 0;
}