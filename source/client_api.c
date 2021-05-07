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
#include "client_api_macros.h"

//static variables
/* the connection file (socket) is stored as a static variable hidden into this file. */
static int conn = -1;

int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	if(conn >= 0) ERRSET(EISCONN, -1);

	size_t len; SOCKNAME_VALID(sockname, &len);
	conn = socket(AF_UNIX, SOCK_STREAM, 0);
	if(conn == -1) return -1;

	//generate connection address
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockname, len + 1);

	//timespec struct to wait
	struct timespec wait_timer, elp_time;
	long tot_time;
	wait_timer.tv_sec = 0;
	wait_timer.tv_nsec = msec * 1000;
	tot_time = 0;

	while(connect(conn, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1)
	{
		if(errno == EAGAIN)
		{
			//check if you are out of time
			if(tot_time > abstime.tv_sec * 1000000000 + abstime.tv_nsec)\
				ERRSET(ETIMEDOUT, -1);

			//retry
			if(nanosleep(&wait_timer, &elp_time) == -1) return -1;
			tot_time += elp_time.tv_nsec;
		}
		else return -1;
	}

	//send connection packet, otherwise connection is closed!
	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_OPEN_CONN);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg);

	int check = CHECK_MSG_CNT(&msg, MESSAGE_OCONN_ACK);
	destroy_message(&msg);
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
	SOCK_VALID(conn);

	//send close message, just to be polite with the server.
	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_CLOSE_CONN);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg);
	destroy_message(&msg);
	//we wait for the server answer just to be sure it has received our message.

	return close(conn);
}

int openFile(const char* pathname, int flags)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	flags &= O_CREATE | O_LOCK;	//make sure there are only valid flags to send.

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_OPEN_FILE);
	write_buf(&msg.data, sizeof(int), &flags);
	write_buf(&msg.data, sizeof(size_t), &len);
	write_buf(&msg.data, len, pathname);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg);


	int check = CHECK_MSG_CNT(&msg, MESSAGE_OFILE_ACK);
	destroy_message(&msg);
	if(check)
	{
		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_OP_SUCC))
			return 0;
		else if(HASFLAG(flags, MESSAGE_FILE_EXISTS))
			{ ERRSET(EEXIST, -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS))
			{ ERRSET(ENOENT, -1); }
		else
			{ ERRSET(EBADMSG, -1); }
	}
	else { ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
}

int readFile(const char* pathname, void** buf, size_t* size)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_READ_FILE);
	write_buf(&msg.data, sizeof(size_t), &len);
	write_buf(&msg.data, len, pathname);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg);

	if(CHECK_MSG_CNT(&msg, MESSAGE_READ_DATA))
	{ 
		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_OP_SUCC))
			;
		else if(HASFLAG(flags, MESSAGE_FILE_NPERM))
			{ ERRSET(EPERM, -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS))
			{ ERRSET(ENOENT, -1); }
		else
			{ ERRSET(EBADMSG, -1); }
		
		ERRCHECKDO(read_buf(&msg.data, sizeof(size_t), size), destroy_message(&msg));
		ERRCHECKDO(read_buf(&msg.data, *size, buf), destroy_message(&msg));
		destroy_message(&msg);
		return 0;
	}
	else { ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
}

int writeFile(const char* pathname, const char* dirname)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len);

}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len);

}

int lockFile(const char* pathname)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_LOCK_FILE);
	write_buf(&msg.data, sizeof(size_t), &len);
	write_buf(&msg.data, len, pathname);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg);

	int check = CHECK_MSG_CNT(&msg, MESSAGE_LFILE_ACK);
	destroy_message(&msg);
	if(check)
	{
		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_OP_SUCC))
			return 0;
		else if(HASFLAG(flags, MESSAGE_FILE_NOWN))
			{ ERRSET(EPERM, -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS))
			{ ERRSET(ENOENT, -1); }
		else
			{ ERRSET(EBADMSG, -1); }
	}
	else { ERRSET(EBADMSG, -1); }
}

int unlockFile(const char* pathname)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_UNLOCK_FILE);
	write_buf(&msg.data, sizeof(size_t), &len);
	write_buf(&msg.data, len, pathname);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg);

	int check = CHECK_MSG_CNT(&msg, MESSAGE_ULFILE_ACK);
	destroy_message(&msg);
	if(check)
	{
		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_OP_SUCC))
			return 0;
		else if(HASFLAG(flags, MESSAGE_FILE_NOWN))
			{ ERRSET(EPERM, -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS))
			{ ERRSET(ENOENT, -1); }
		else
			{ ERRSET(EBADMSG, -1); }
	}
	else { ERRSET(EBADMSG, -1); }
}

int closeFile(const char* pathname)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_CLOSE_FILE);
	write_buf(&msg.data, sizeof(size_t), &len);
	write_buf(&msg.data, len, pathname);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg);

	int check = CHECK_MSG_CNT(&msg, MESSAGE_CFILE_ACK);
	destroy_message(&msg);
	if(check)
	{
		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_OP_SUCC))
			return 0;
		else if(HASFLAG(flags, MESSAGE_FILE_LOCK))
			{ ERRSET(EPERM, -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS))
			{ ERRSET(ENOENT, -1); }
		else
			{ ERRSET(EBADMSG, -1); }
	}
	else { ERRSET(EBADMSG, -1); }
}

int removeFile(const char* pathname)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_REMOVE_FILE);
	write_buf(&msg.data, sizeof(size_t), &len);
	write_buf(&msg.data, len, pathname);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg);

	int check = CHECK_MSG_CNT(&msg, MESSAGE_RFILE_ACK);
	destroy_message(&msg);
	if(check)
	{
		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_OP_SUCC))
			return 0;
		else if(HASFLAG(flags, MESSAGE_FILE_NOWN) || HASFLAG(flags, MESSAGE_FILE_NLOCK))
			{ ERRSET(EPERM, -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS))
			{ ERRSET(ENOENT, -1); }
		else
			{ ERRSET(EBADMSG, -1); }
	}
	else { ERRSET(EBADMSG, -1); }
}
