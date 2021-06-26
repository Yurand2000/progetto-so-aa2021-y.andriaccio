#include "net_msg.h"
#include "win32defs.h"

#include <unistd.h>
#include <errno.h>

#include "hash.h"
#include "errset.h"
#include "readn_writen.h"

typedef struct msg_header
{
	uint32_t type;
	size_t len;
	uint64_t checksum;
} msg_header;

//write and read msg heads and body to file
//return 0 on success, -1 on error, sets errno.
//write_msghead and read_msghead return 1 if body needs to be wrote.
static int write_msghead(int fileno, const net_msg* msg);
static int write_msgbody(int fileno, const net_msg* msg);

static int read_msghead(int fileno, net_msg* msg, size_t* read_size);
static int read_msgbody(int fileno, net_msg* msg, size_t read_size);

int create_message(net_msg* msg)
{
	if(msg == NULL) ERRSET(EINVAL, -1);

	msg->type = NET_MESSAGE_NULL;
	create_data_buffer(&(msg->data));
	msg->checksum = 0;
	return 0;
}

int destroy_message(net_msg* msg)
{
	if(msg == NULL) ERRSET(EINVAL, -1);

	destroy_data_buffer(&(msg->data));
	return 0;
}

int set_checksum(net_msg* msg)
{
	if(msg == NULL) ERRSET(EINVAL, -1);

	msg->checksum = hash(msg->data.buffer, msg->data.buf_size);
	return 0;
}

int check_checksum(const net_msg* msg)
{
	if(msg == NULL) ERRSET(EINVAL, -1);

	if(msg->checksum == hash(msg->data.buffer, msg->data.buf_size))
		return 0;
	else
		return 1;
}

int write_msg(int fileno, const net_msg* msg)
{
	if(msg == NULL) ERRSET(EINVAL, -1);

	int err;
	err = write_msghead(fileno, msg);
	if(err == -1) return -1;
	else if(err == 0) return 0;

	err = write_msgbody(fileno, msg);
	return err;
}

int read_msg(int fileno, net_msg* msg)
{
	if(msg == NULL) ERRSET(EINVAL, -1);

	int err; size_t read_size;
	err = read_msghead(fileno, msg, &read_size);
	if(err == -1) return -1;
	else if(err == 0) return 0;

	err = read_msgbody(fileno, msg, read_size);
	return err;
}

static int write_msghead(int fileno, const net_msg* msg)
{
	msg_header head;
	head.type = msg->type;
	head.checksum = msg->checksum;
	head.len = msg->data.buf_size;
	ssize_t err = writen(fileno, &head, sizeof(msg_header));

	if(err == -1) return -1;
	if(err != sizeof(msg_header)) ERRSET(EMSGSIZE, -1);

	if(head.len == 0)
		return 0;
	else
		return 1;
}

static int write_msgbody(int fileno, const net_msg* msg)
{
	ssize_t err = writen(fileno, msg->data.buffer, msg->data.buf_size);
	
	if(err == -1) return -1;
	if(err != msg->data.buf_size) ERRSET(EMSGSIZE, -1);
	return 0;
}

static int read_msghead(int fileno, net_msg* msg, size_t* read_size)
{
	msg_header head;
	ssize_t err = readn(fileno, &head, sizeof(msg_header));
	
	if(err == -1) return -1;
	if(err != sizeof(msg_header)) ERRSET(EMSGSIZE, -1);

	msg->type = head.type;
	msg->checksum = head.checksum;
	*read_size = head.len;
	if(head.len == 0)
		return 0;
	else
		return 1;
}

static int read_msgbody(int fileno, net_msg* msg, size_t read_size)
{
	if(resize_data_buffer(&msg->data, read_size) == -1)
		return -1;
	
	ssize_t err = readn(fileno, msg->data.buffer, read_size);
	
	if(err == -1) return -1;
	if(err != read_size) ERRSET(EMSGSIZE, -1);

	msg->data.buf_size = read_size;
	return 0;
}
