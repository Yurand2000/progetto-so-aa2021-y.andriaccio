#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#include "../source/client/client_subcalls.h"
#include "../source/client_api.h"
#include "../source/worker/worker_generics.h"

int main(int argc, char* argv[])
{
	char* socket_name = NULL;
	char currdir[FILENAME_MAX]; size_t currdir_size;
	req_t* reqs = NULL; size_t curr_reqs = 0, reqs_size = 0;
	int do_print = 0; int time_between_reqs = 0;

	PERRCHECK(get_cwd(&currdir, &currdir_size), "CWD Unexpected error");

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

	//clean and close ---------------------------------------------------------
	for (size_t i = 0; i < curr_reqs; i++)
		destroy_request(&reqs[i]);
	free(reqs);

	return 0;
}