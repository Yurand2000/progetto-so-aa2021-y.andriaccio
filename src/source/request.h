#ifndef CLIENT_REQUEST
#define CLIENT_REQUEST

#define REQUEST_NULL 0
#define REQUEST_READ 1
#define REQUEST_READN 2
#define REQUEST_WRITE 3
#define REQUEST_WRITE_DIR 4
#define REQUEST_LOCK 5
#define REQUEST_UNLOCK 6
#define REQUEST_REMOVE 7
#define REQUEST_OPEN 10
#define REQUEST_CREATE_LOCK 20
#define REQUEST_CLOSE 30

#include <stdlib.h>

/* this structure serves as support for the client application to know what requests to send.
 * after it is fully setup, you just need to traverse it like an array to get the requests in
 * order. */

typedef struct request {
	int type;
	int n;
	char* stringdata;
	size_t stringdata_len;
	char* dir;
	size_t dir_len;
} req_t;

//init the request structure, returns 0 on success, -1 on failure and sets errno.
int init_request(req_t* req);
//destroys the request structure, returns 0 on success, -1 on failure and sets errno.
int destroy_request(req_t* req);

//adds a request to a request array, reallocaing it if necessary.
//returns 0 on success, -1 on failure and sets errno.
//On memory error it destroys the whole array.
int add_request(req_t newreq, req_t** reqs, size_t* curr_reqs, size_t* reqs_size);

#endif
