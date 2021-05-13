#ifndef CLIENT_REQUEST
#define CLIENT_REQUEST

#define REQUEST_NULL 0
#define REQUEST_OPEN 1
#define REQUEST_CREATE_LOCK 2
#define REQUEST_CLOSE 3
#define REQUEST_READ 4
#define REQUEST_WRITE 5
#define REQUEST_LOCK 6
#define REQUEST_UNLOCK 7
#define REQUEST_REMOVE 8

#include <stdlib.h>

/* this structure serves as support for the client application to know what requests to send.
 * after it is fully setup, you just need to traverse it like an array to get the requests in
 * order. */

typedef struct request {
	int type;
	char* filename;
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
