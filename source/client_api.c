#include "client_api.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "errset.h"
#include "net_msg.h"
#include "message_type.h"
#define UNIX_PATH_MAX 108
#define FILE_PATH_MAX 256

static int conn = -1;

int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	if(conn >= 0) ERRSET(EISCONN, -1);

	size_t len = strlen(sockname);
	if(len >= UNIX_PATH_MAX)
		ERRSET(ENAMETOOLONG, -1);

	conn = socket(AF_UNIX, SOCK_STEAM, 0);
	if(conn == -1) return -1;

	//generate connection address
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(sa.sun_path, sockname, len + 1);

	//timespec struct to wait
	struct timespec wait_timer, elp_time;
	long tot_time;
	wait_time.tv_sec = 0;
	wait_time.tv_nsec = msec * 1000;
	tot_time = 0

	while(connect(conn, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1)
	{
		if(errno == EAGAIN)
		{
			//check if you are out of time
			if(tot_time.tv_nsec > abstime.tv_sec * 1000000000 + abstime.tv_nsec)\
				ERRSET(ETIMEDOUT, -1);

			//retry
			if(nanosleep(&wait_timer, &elp_time) == -1) return -1;
			tot_time += elp_time.tv_nsec;
		}
		else return -1;
	}

	//send connection packet, otherwise connection is closed!
	net_msg msg;
	create_message(&msg);
	msg.type = MESSAGE_OPEN_CONN;
	set_checksum(&msg);
	ERRCHECKDO(write_msg(conn, &msg), delete_message(&msg));

	delete_message(&msg);
	create_message(&msg);
	ERRCHECKDO(read_msg(conn, &msg), delete_message(&msg));

	int check = check_checksum(&msg) && msg.type == MESSAGE_OCONN_ACK;
	delete_message(&msg);
	if(check)
		return 0;
	else
	{
		ERRCHECK(close(conn));
		ERRSET(ENETRESET, -1);
	}
}

int closeConnection(const char* sockname)
{
	if(conn < 0) ERRSET(ENOTCONN, -1);

	//send close message, just to be polite with the server.
	net_msg msg;
	create_message(&msg);
	msg.type = MESSAGE_CLOSE_CONN;
	set_checksum(&msg);
	ERRCHECKDO(write_msg(conn, &msg), delete_message(&msg));

	delete_message(&msg);
	/* we don't really need to wait for the server answer, it is just to not abruptly disconnect.
	create_message(&msg);
	ERRCHECKDO(read_msg(conn, &msg), delete_message(&msg));

	int check = check_checksum(&msg) && msg.type == MESSAGE_CCONN_ACK;
	delete_message(&msg);
	*/

	return close(conn);
}

int openFile(const char* pathname, int flags)
{
	size_t len = strlen(pathname);
	if(len > FILE_PATH_MAX) ERRSET(ENAMETOOLONG, -1);

	flags &= O_CREATE | O_LOCK;	//make sure there are only valid flags to send.

	net_msg msg, answ;
	create_message(&msg);
	msg.type = MESSAGE_OPEN_FILE;
	write_buf(&msg.data, sizeof(int), &flags);
	write_buf(&msg.data, len + 1, pathname);
	set_checksum(&msg);

	ERRCHECKDO(write_msg(conn, &msg), delete_message(&msg));
	delete_message(&msg);

	create_message(&msg);
	ERRCHECKDO(read_msg(conn, &msg), delete_message(&msg));

	int check = check_checksum(&msg) && msg.type == MESSAGE_OFILE_ACK;
	delete_message(&msg);

	if(check)
	{
		int ret_cond;
		read_buf(&msg.data, sizeof(int), &ret_cond);
		switch(ret_cond)
		{
		case MESSAGE_OFILE_SUCCESS:
			return 0;
		case MESSAGE_OFILE_EXIST:
			ERRSET(EEXIST, -1);
		case MESSAGE_OFILE_NEXIST:
			ERRSET(ENOENT, -1);
		default:
			ERRSET(EBADMSG, -1);
		}
	}
	else ERRSET(EBADMSG, -1);
}

int readFile(const char* pathname, void** buf, size_t size)
{

}

int writeFile(const char* pathname, const char* dirname)
{

}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{

}

int lockFile(const char* pathname)
{

}

int closeFile(const char* pathname)
{

}

int removeFile(const char* pathname)
{

}
