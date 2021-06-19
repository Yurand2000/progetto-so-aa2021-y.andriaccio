#ifndef CLIENT_MACROS
#define CLIENT_MACROS
#include "../source/win32defs.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

#include "../source/errset.h"
#include "../source/request.h"

#define CMD_TO_REQUEST(__type__) {\
	req_t request;\
	init_request(&request);\
	\
	request.type = __type__;\
	if(__type__ == REQUEST_READN)\
	{\
		request.n = strtol(optarg, NULL, 0);\
	}\
	else if (__type__ == REQUEST_WRITE_DIR)\
	{\
		char* saveptr = NULL;\
		char* token = strtok_r(optarg, ",", &saveptr);\
		request.stringdata_len = strlen(token);\
		MALLOC(request.stringdata, request.stringdata_len);\
		strncpy(request.stringdata, token, request.stringdata_len);\
		\
		token = strtok_r(NULL, ",", &saveptr);\
		request.n = strtol(token, NULL, 0);\
	}\
	else\
	{\
		request.stringdata_len = strlen(optarg);\
		MALLOC(request.stringdata, request.stringdata_len);\
		strncpy(request.stringdata, optarg, request.stringdata_len);\
	}\
	\
	ERRCHECK(add_request(request, reqs, curr_reqs, reqs_size));\
}

#define WRITE_DIR_TO_LAST_REQUEST {\
	req_t* last = &(*reqs)[*curr_reqs - 1];\
	last->dir_len = strlen(optarg);\
	MALLOC(last->dir, last->dir_len);\
	strncpy(last->dir, optarg, last->dir_len);\
}

#define ADD_REQUESTS(_type_) {\
	req_t request;\
	char* saveptr = NULL;\
	char* token = strtok_r(optarg, ",", &saveptr);\
	while(token != NULL)\
	{\
		init_request(&request);\
		request.type = _type_;\
		size_t len = strlen(token); len++;\
		MALLOC((request.filename), (sizeof(char) * len));\
		strncpy(request.filename, token, len);\
		\
		/*add request to array*/\
		add_request(request, reqs, curr_reqs, reqs_size);\
		\
		/*destroy_request(&request);*/\
		/*request is not destroyed since pointers*/\
		/*are copied (not their contents)*/\
		\
		token = strtok_r(NULL, ",", &saveptr);\
	}\
}\

#endif
