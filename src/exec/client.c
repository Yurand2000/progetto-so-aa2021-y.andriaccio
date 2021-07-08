#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#define CLIENT_API_ENABLE

#include "../source/client_api.h"
#include "../source/worker/worker_generics.h"

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

	//add open/create and close requests. -------------------------------------
	curr_reqs = 0;	//using the old reqs array, but as if it was empty.
					//all old memory has already been cleared.

	add_open_create_requests(reqs_exp, curr_reqs_exp, &reqs, &curr_reqs, &reqs_size);
	free(reqs_exp);

	//call the client api -----------------------------------------------------
#ifdef CLIENT_API_ENABLE
#define DO_PRINT(op, file, res) if (do_print) print_operation_result(op, file, res);

	int res; FILE* fd;
	void* buf = NULL; size_t size = 0;

	//connect
	struct timespec abstime; abstime.tv_sec = 5; abstime.tv_nsec = 0; long nsec;
	if (time_between_reqs > 100)
		res = openConnection(socket_name, time_between_reqs, abstime);
	else
		res = openConnection(socket_name, 500, abstime);

	if(res == -1)
	{
		printf("couldn't connect to server!\n");
		return -1;
	}

	//pass requests
	char timestamp[23];
	nsec = (long)time_between_reqs * 1000000L;
	struct timespec waittime; waittime.tv_sec = nsec / 1000000000L; waittime.tv_nsec = nsec % 1000000000L;
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
				ERRCHECK( convert_slashes_to_underscores(reqs[i].stringdata) );
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
			buf = NULL;
			size = 0;
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
			{
				time_t currtime; struct tm time_print;
				time(&currtime);
				localtime_r(&currtime, &time_print);

				snprintf(timestamp, sizeof(timestamp), "[%04d/%02d/%02d-%02d:%02d:%02d]\n",
					(time_print.tm_year + 1900), (time_print.tm_mon + 1), time_print.tm_mday,
					time_print.tm_hour, time_print.tm_min, time_print.tm_sec);
			}
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

	closeConnection(socket_name);
#endif

	//clean and close
	for (size_t i = 0; i < curr_reqs; i++)
		destroy_request(&reqs[i]);
	free(reqs);

	return 0;
}