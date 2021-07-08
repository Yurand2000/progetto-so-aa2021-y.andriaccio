#include "client_subcalls.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

//used by the parser to add requests
static int cmd_to_request(int type, req_t** reqs, size_t* curr_reqs, size_t* reqs_size);

//used by the parser for the -d and -D options
static int write_dir_to_last_request(req_t** reqs, size_t* curr_reqs);

int parse_args(int argc, char* argv[], char** socket_name, int* do_print,
	req_t** reqs, size_t* curr_reqs, size_t* reqs_size, int* time_between_reqs)
{
	int option, prec = 0;
	while ((option = getopt(argc, argv, "-:hf:w:W:a:D:r:R::d:t:l:u:c:p")) != -1)
	{
		switch (option)
		{
		case 'h':
			print_help();
			return 1;
		case 'f':
			if (*socket_name != NULL)
			{
				printf("-f flag is specified more than once. "
					"Start with -h for details.\n");
				return -1;
			}
			*socket_name = optarg;
			break;
		case 'p':
			if (*do_print == 1)
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
		case 'a':
			ERRCHECK(cmd_to_request(REQUEST_APPEND, reqs, curr_reqs, reqs_size));
			break;
		case 'D':
			if (prec != 'w' && prec != 'W' && prec != 'a')
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

int expand_dir_to_files(char* dirname, int max, char* retdir, size_t retdir_size,
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

int split_and_fix_request_files(req_t* req, req_t** reqs,
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

int add_open_create_requests(req_t* reqs, size_t reqs_num, req_t** out_reqs,
	size_t* out_curr_reqs, size_t* out_reqs_size)
{
	req_t temp;
	init_request(&temp);

	for (size_t i = 0; i < reqs_num; i++)
	{
		req_t* req = &reqs[i];

		//insert the open request
		if (req->type == REQUEST_READ || req->type == REQUEST_LOCK ||
			req->type == REQUEST_UNLOCK || req->type == REQUEST_APPEND)
		{
			int insert = 0;
			for (int j = *out_curr_reqs - 1; j >= 0 && insert == 0; j--)
			{
				if ((*out_reqs)[j].type == REQUEST_REMOVE
					&& strcmp((*out_reqs)[j].stringdata, req->stringdata) == 0) insert = 1;
				if (((*out_reqs)[j].type == REQUEST_OPEN || (*out_reqs)[j].type == REQUEST_OPEN_LOCK
					|| (*out_reqs)[j].type == REQUEST_CREATE_LOCK)
					&& strcmp((*out_reqs)[j].stringdata, req->stringdata) == 0) insert = -1;
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
				|| req->type == REQUEST_WRITE || req->type == REQUEST_APPEND)
			{
				int insert = 0;
				for (int j = i + 1; j < reqs_num; j++)
				{
					if (reqs[j].stringdata != NULL && strcmp(reqs[j].stringdata, req->stringdata) == 0) insert = 1;
				}
				for (int j = *out_curr_reqs - 1; j >= 0 && insert == 0; j--)
				{
					if ((*out_reqs)[j].type == REQUEST_REMOVE
						&& strcmp((*out_reqs)[j].stringdata, req->stringdata) == 0) insert = 1;
					if (((*out_reqs)[j].type == REQUEST_OPEN || (*out_reqs)[j].type == REQUEST_OPEN_LOCK
						|| (*out_reqs)[j].type == REQUEST_CREATE_LOCK)
						&& strcmp((*out_reqs)[j].stringdata, req->stringdata) == 0) insert = -1;
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
	if (type == REQUEST_READN)
	{
		if (optarg != NULL)
		{
			char* saveptr = NULL;
			char* token = strtok_r(optarg, "=", &saveptr);
			token = strtok_r(NULL, " ", &saveptr);
			request.n = strtol(token, NULL, 0);
		}
	}
	else if (type == REQUEST_WRITE_DIR)
	{
		char* saveptr = NULL;
		char* token = strtok_r(optarg, ",", &saveptr);
		request.stringdata_len = strlen(token) + 1;
		MALLOC(request.stringdata, request.stringdata_len);
		strncpy(request.stringdata, token, request.stringdata_len);

		token = strtok_r(NULL, "=", &saveptr);
		token = strtok_r(NULL, " ", &saveptr);
		if (token != NULL) request.n = strtol(token, NULL, 0);
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