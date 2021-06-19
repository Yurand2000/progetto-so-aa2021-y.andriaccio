#ifndef NETWORK_MESSAGE
#define NETWORK_MESSAGE

#include <stdlib.h>
#include <stdint.h>
#include "databuf.h"

#define NET_MESSAGE_NULL (uint32_t)(0)

typedef struct network_message {
	uint32_t type;
	databuf data;
	uint64_t checksum;
} net_msg;

/* all the functions except write_msg and read_msg can be safely not error checked if
 * the pointer to net_msg struct is a valid pointer. */

/* initializes the net_msg structure
 * returns 0 on success, -1 on error and sets errno */
int create_message(net_msg* msg);

/* destroys the net_msg structure
 * returns 0 on success and -1 on error, sets errno */
int destroy_message(net_msg* msg);

/* calculates the checksum of the whole message and sets it
 * returns 0 on success and -1 on error, sets errno */
int set_checksum(net_msg* msg);

/* validates the message checksum
 * returns 0 on successful check, 1 otherwise. returns -1 and sets errno on error */
int check_checksum(const net_msg* msg);

/* writes the net_msg to a given file descriptor, make sure it is open.
 * returns 0 on success, -1 on error and sets errno */
int write_msg(int fileno, const net_msg* msg);

/* reads a net_msg from a given file descriptor, make sure it is open.
 * returns 0 on success, -1 on error and sets errno */
int read_msg(int fileno, net_msg* msg);

#endif
