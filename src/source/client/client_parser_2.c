#include "client_subcalls.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

//expands the given directory to its files in requests.
static int expand_dir_to_files(char* dirname, int max, char* retdir, size_t retdir_size,
	req_t** reqs, size_t* curr_reqs, size_t* reqs_size,
	int* count_ptr, const char* currdir, size_t currdir_size);

//split multifile requests and changes local to absolute path
static int split_and_fix_request_files(req_t* req, req_t** reqs,
	size_t* curr_reqs, size_t* reqs_size, const char* currdir, size_t currdir_size);

int expand_requests(req_t** reqs, size_t* curr_reqs, size_t* reqs_size,
	char currdir[], size_t currdir_size)
{
	req_t* reqs_exp = NULL; size_t curr_reqs_exp = 0, reqs_size_exp = 0;
	for (size_t i = 0; i < *curr_reqs; i++)
	{
		req_t* curr = &(*reqs)[i];
		if (curr->type == REQUEST_WRITE_DIR)
		{
			expand_dir_to_files(
				curr->stringdata, curr->n,
				curr->dir, curr->dir_len,
				&reqs_exp, &curr_reqs_exp, &reqs_size_exp,
				NULL, currdir, currdir_size);
			destroy_request(curr);
		}
		else if (curr->type == REQUEST_READN)
		{
			add_request(*curr, &reqs_exp, &curr_reqs_exp, &reqs_size_exp);
		}
		else
		{
			split_and_fix_request_files(curr, &reqs_exp, &curr_reqs_exp,
				&reqs_size_exp, currdir, currdir_size);
			destroy_request(curr);
		}
	}

	free(*reqs);
	*reqs = reqs_exp;
	*curr_reqs = curr_reqs_exp;
	*reqs_size = reqs_size_exp;
	return 0;
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