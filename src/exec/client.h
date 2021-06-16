#ifndef CLIENT_MACROS
#define CLIENT_MACROS
#include "../source/errset.h"
#include "../source/request.h"

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
