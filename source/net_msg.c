#include "net_msg.h"

//write and read msg heads and body to file
//return 0 on success, -1 on error, sets errno.
//write_msghead and read_msghead return 1 if body needs to be wrote.
static int write_msghead(int fileno, const net_msg* msg);
static int write_msgbody(int fileno, const net_msg* msg);

static int read_msghead(int fileno, net_msg* msg);
static int read_msgbody(int fileno, net_msg* msg);



static int write_msghead(int fileno, const net_msg* msg)
{

}

static int write_msgbody(int fileno, const net_msg* msg)
{

}

static int read_msghead(int fileno, net_msg* msg)
{

}

static int read_msgbody(int fileno, net_msg* msg)
{

}
