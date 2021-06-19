#include "request.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "errset.h"

int init_request(req_t* req)
{
	if(req == NULL) ERRSET(EINVAL, -1);

	req->type = REQUEST_NULL;
	req->n = 0;
	req->stringdata = NULL;
	req->stringdata_len = 0;
	req->dir = NULL;
	req->dir_len = 0;
	return 0;
}

int destroy_request(req_t* req)
{
	if(req == NULL) ERRSET(EINVAL, -1);
	free(req->stringdata);
	free(req->dir);
	return 0;
}

int add_request(req_t newreq, req_t** reqs, size_t* curr_reqs, size_t* reqs_size)
{
	if(reqs == NULL || curr_reqs == NULL || reqs_size == NULL) ERRSET(EINVAL, -1);
	if(*curr_reqs >= *reqs_size)
	{
		//reallocate the array here
		req_t* new_reqs = NULL;
		REALLOCDO(new_reqs, (*reqs), sizeof(req_t) * (*reqs_size + 10), 
			{
				for(size_t i = 0; i < *reqs_size; i++)
					destroy_request(&(*reqs)[i]);
				free(*reqs);
				*reqs = NULL;
				*curr_reqs = 0;
				*reqs_size = 0;
			} );
		*reqs = new_reqs;
		*reqs_size += 10;
	}

	init_request( &((*reqs)[*curr_reqs]) );
	((*reqs)[*curr_reqs]).type = newreq.type;
	((*reqs)[*curr_reqs]).n = newreq.n;
	((*reqs)[*curr_reqs]).stringdata = newreq.stringdata;
	((*reqs)[*curr_reqs]).stringdata_len = newreq.stringdata_len;
	((*reqs)[*curr_reqs]).dir = newreq.dir;
	((*reqs)[*curr_reqs]).dir_len = newreq.dir_len;
	(*curr_reqs)++;
	return 0;
}
