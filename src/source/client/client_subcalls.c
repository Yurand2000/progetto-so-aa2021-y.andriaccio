#include "client_subcalls.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#include "../client_api.h"
#include "../worker/worker_generics.h"

void print_help()
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
"-a file1[,file2]      - Append a time stamp to the given file[s]."\
"[-D dirname]          - This option goes only with -w, -W or -a. Specifies a\n"\
"                        folder to write cachemiss files expelled from the\n"\
"                        server. It is optional.\n"\
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

void print_operation_result(const char* op_type, const char* file, int res)
{
	char* op_res = (res != -1) ? "Success" : strerror(errno);

	int bytes_read, bytes_write;
	get_bytes_read_write(&bytes_read, &bytes_write);
	printf("Operation: %s; File: %s; Result: %s", op_type, file, op_res);
	if (res > 0)
		printf("; Files read: %d", res);
	if (bytes_read != 0 || bytes_write != 0)
		printf("; Bytes: read %d; write %d", bytes_read, bytes_write);
	printf(".\n");
}

int get_cwd(char currdir[], size_t* currdir_size)
{
	PTRCHECK(getcwd(currdir, FILENAME_MAX - 1));
	*currdir_size = strlen(currdir);

	//add trailing / if necessary
	if (currdir[(*currdir_size) - 1] != '/')
	{
		currdir[(*currdir_size)] = '/';
		currdir[(*currdir_size) + 1] = '\0';
		(*currdir_size)++;
	}
	return 0;
}

int check_socket_file(char* socket_name)
{
	if (socket_name == NULL || socket_name[0] == '\0')
	{
		printf("-f flag is mandatory. start with -h for details.\n");
		ERRSET(ENOENT, -1);
	}
	else
	{
		ERRCHECKDO(access(socket_name, R_OK | W_OK),
			printf("socket file not found. Has the server started?\n"));
	}
	return 0;
}

#define DO_PRINT(op, file, res) if (do_print) print_operation_result(op, file, res);

int open_connection(char* socket_name, int time_between_reqs)
{
	struct timespec abstime; abstime.tv_sec = 5; abstime.tv_nsec = 0; int res = 0;
	if (time_between_reqs > 100)
		res = openConnection(socket_name, time_between_reqs, abstime);
	else
		res = openConnection(socket_name, 500, abstime);
	return res;
}

static void data_to_file(void* buf, size_t size, req_t* req)
{
	FILE* fd;
	if (req->dir != NULL)
	{
		//create file and write buf data
		size_t dirlen = strlen(req->dir);
		if (convert_slashes_to_underscores(req->stringdata) == -1)
		{
			perror("Write to file failure [0]");
			return;
		}
		size_t len = strlen(req->stringdata) + 1;
		char* file = malloc(len + dirlen);
		if (file != NULL)
		{
			strncpy(file, req->dir, dirlen);
			strncpy(file + dirlen, req->stringdata, len);
			fd = fopen(file, "wb");
			if (fd != NULL)
			{
				fwrite(buf, sizeof(char), size, fd);
				fclose(fd);
			}
			else perror("Write to file failure [1]");
			free(file);
		}
		else perror("Write to file failure [2]");
	}
}

static void calc_timestamp(char* timestamp, size_t timestamp_size)
{
	time_t currtime; struct tm time_print;
	time(&currtime);
	localtime_r(&currtime, &time_print);

	snprintf(timestamp, timestamp_size, "[%04d/%02d/%02d-%02d:%02d:%02d]\n",
		(time_print.tm_year + 1900), (time_print.tm_mon + 1), time_print.tm_mday,
		time_print.tm_hour, time_print.tm_min, time_print.tm_sec);
}

int send_requests(req_t* reqs, size_t curr_reqs, int time_between_reqs, int do_print)
{
	int res; struct timespec waittime;
	char timestamp[23]; long nsec;
	void* buf = NULL; size_t size = 0;

	nsec = (long)time_between_reqs * 1000000L;	
	waittime.tv_sec = nsec / 1000000000L;
	waittime.tv_nsec = nsec % 1000000000L;
	
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
			if (res != -1) data_to_file(buf, size, &reqs[i]);
			free(buf); buf = NULL; size = 0;
			break;
		case REQUEST_READN:
			res = readNFiles(reqs[i].n, reqs[i].dir);
			DO_PRINT("Read N Files", reqs[i].stringdata, res);
			break;
		case REQUEST_WRITE:
			res = writeFile(reqs[i].stringdata, reqs[i].dir);
			DO_PRINT("Write File", reqs[i].stringdata, res);
			break;
		case REQUEST_APPEND:
			calc_timestamp(timestamp, sizeof(timestamp));
			res = appendToFile(reqs[i].stringdata, timestamp, sizeof(timestamp), reqs[i].dir);
			DO_PRINT("Append to File", reqs[i].stringdata, res);
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

	return 0;
}