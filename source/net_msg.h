#ifndef NETWORK_MESSAGE
#define NETWORK_MESSAGE

#include <stdlib.h>
#include <stdint.h>
#include "databuf.h"

typedef struct network_message {
	uint32_t type;
	databuf data;
	uint64_t checksum;
} net_msg;

int create_message(net_msg* msg);

int destroy_message(net_msg* msg);

int set_checksum(net_msg* msg);

int check_checksum(const net_msg* msg);

size_t get_message_size(const net_msg* msg);

int write_msg(int fileno, const net_msg* msg);
int read_msg(int fileno, net_msg* msg);

#endif
