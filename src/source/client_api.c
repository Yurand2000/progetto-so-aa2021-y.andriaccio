#include "client_api.h"
#include "win32defs.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "errset.h"
#include "net_msg.h"
#include "message_type.h"
#include "net_msg_macros.h"
#include "readn_writen.h"

/* the connection file (socket) is stored as a static variable hidden into this file. */
static int conn = -1;

static int save_cached_files(net_msg* msg, const char* dirname);

static int write_buf_to_file(char const* filename, char* buf, size_t filesize, const char* dirname);

#define CLOSE { close(conn); conn = -1; }

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
			if(tot_time > abstime.tv_sec * 1000000000 + abstime.tv_nsec)
				ERRSETDO(ETIMEDOUT, CLOSE, -1);

			//retry wait time
			ERRCHECKDO(nanosleep(&wait_timer, &elp_time), { CLOSE; return -1; });
			tot_time += elp_time.tv_nsec;
		}
		else
		{
			close(conn);
			conn = -1;
			return -1;
		}
	}

	//send connection packet, otherwise connection is closed!
	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_OPEN_CONN);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg, CLOSE);

	int check = CHECK_MSG_CNT(&msg, MESSAGE_OCONN_ACK);
	destroy_message(&msg);
	if(check)
		return 0;
	else
	{
		close(conn);
		conn = -1;
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

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg, CLOSE);
	destroy_message(&msg);
	//we wait for the server answer just to be sure it has received our message.

	return close(conn);
}

int openFile(const char* pathname, int flags)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	msg_t flg = MESSAGE_FLAG_NONE;
	if(flags & O_CREATE) flg |= MESSAGE_OPEN_OCREATE;
	if(flags & O_LOCK) flg |= MESSAGE_OPEN_OLOCK;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_OPEN_FILE | flg);
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg, CLOSE);

	int check = CHECK_MSG_CNT(&msg, MESSAGE_OFILE_ACK);
	destroy_message(&msg);
	if(check)
	{
		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_OP_SUCC))
			return 0;
		else if(HASFLAG(flags, MESSAGE_FILE_NPERM))
			{ ERRSET(EPERM, -1); }
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
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg, CLOSE);

	if(CHECK_MSG_CNT(&msg, MESSAGE_RFILE_ACK))
	{ 
		msg_t flags = GETFLAGS(msg.type);
		if (HASFLAG(flags, MESSAGE_OP_SUCC))
		{
			ERRCHECKDO(pop_buf(&msg.data, sizeof(size_t), size), destroy_message(&msg));

			MALLOCDO(*buf, (sizeof(char) * (*size)), destroy_message(&msg));

			ERRCHECKDO(pop_buf(&msg.data, *size, *buf), { destroy_message(&msg); free(*buf); });

			destroy_message(&msg);
		}
		else if(HASFLAG(flags, MESSAGE_FILE_NPERM))
			{ ERRSET(EPERM, -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS))
			{ ERRSET(ENOENT, -1); }
		else
			{ ERRSET(EBADMSG, -1); }
		
		return 0;
	}
	else { ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
}

int readNFiles(int n, const char* dirname)
{
	SOCK_VALID(conn);

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_READN_FILE);
	push_buf(&msg.data, sizeof(int), &n);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg, CLOSE);

	if (CHECK_MSG_CNT(&msg, MESSAGE_RNFILE_ACK))
	{
		msg_t flags = GETFLAGS(msg.type);
		if (HASFLAG(flags, MESSAGE_OP_SUCC))
		{ ERRCHECKDO(save_cached_files(&msg, dirname), { destroy_message(&msg); }); }
		else
		{ ERRSETDO(EBADMSG, destroy_message(&msg), -1); }

		return 0;
	}
	else { ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
}

int writeFile(const char* pathname, const char* dirname)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(dirname, &len);
	FILENAME_VALID(pathname, &len); len++;

	//open file, check if it exists
	int file; ERRCHECK((file = open(pathname, O_RDONLY)));

	//get file size
	struct stat file_stats;
	ERRCHECKDO(fstat(file, &file_stats), close(file));
	size_t file_size = file_stats.st_size;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_WRITE_FILE);

	//now write the file into the message
	char* buf; MALLOCDO(buf, (sizeof(char) * file_size), destroy_message(&msg));
	ERRCHECKDO(readn(file, buf, file_size), free(buf));

	push_buf(&msg.data, file_size, buf);
	push_buf(&msg.data, sizeof(size_t), &file_size);
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);

	//if last read was 0, then it should have succeded into creating the message
	free(buf);
	close(file);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg, CLOSE);

	int cachemiss = 0;
	if(CHECK_MSG_CNT(&msg, MESSAGE_WFILE_ACK))
	{

		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_FILE_CHACEMISS))
			cachemiss = 1;

		if (HASFLAG(flags, MESSAGE_OP_SUCC))
		{
			if (cachemiss == 1 && dirname != NULL)
				ERRCHECKDO(save_cached_files(&msg, dirname), { destroy_message(&msg); });

			destroy_message(&msg);
			return 0;
		}
		else if(HASFLAG(flags, MESSAGE_FILE_NOWN))
			{ ERRSETDO(EPERM, destroy_message(&msg), -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS))
			{ ERRSETDO(ENOENT, destroy_message(&msg), -1); }
		else
			{ ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
	}
	else { ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_APPEND_FILE);

	//write the appended data into the message
	push_buf(&msg.data, size, buf);
	push_buf(&msg.data, sizeof(size_t), &size);
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);

	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg, CLOSE);

	int cachemiss = 0;
	if(CHECK_MSG_CNT(&msg, MESSAGE_AFILE_ACK))
	{
		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_FILE_CHACEMISS))
			cachemiss = 1;

		if (HASFLAG(flags, MESSAGE_OP_SUCC))
		{
			if (cachemiss == 1 && dirname != NULL)
				ERRCHECKDO(save_cached_files(&msg, dirname), { destroy_message(&msg); });

			destroy_message(&msg);
			return 0;
		}
		else if(HASFLAG(flags, MESSAGE_FILE_NOWN))
			{ ERRSETDO(EPERM, destroy_message(&msg), -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS))
			{ ERRSETDO(ENOENT, destroy_message(&msg), -1); }
		else
			{ ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
	}
	else { ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
}

int lockFile(const char* pathname)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_LOCK_FILE);
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg, CLOSE);

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
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg, CLOSE);

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
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg, CLOSE);

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
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg, CLOSE);

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

static int save_cached_files(net_msg* msg, const char* dirname)
{
	size_t count, len, name_len = 0, buf_len = 0;
	char* buf = NULL; char* name = NULL;

	ERRCHECKDO(pop_buf(&msg->data, sizeof(size_t), &count), { free(name); free(buf); });
	for (size_t i = 0; i < count; i++)
	{
		//get name
		ERRCHECKDO(pop_buf(&msg->data, sizeof(size_t), &len), { free(name); free(buf); });
		if (len > name_len)
		{
			REALLOCDO(name, name, (sizeof(char) * len), { free(name); free(buf); });
			name_len = len;
		}
		ERRCHECKDO(pop_buf(&msg->data, (sizeof(char) * len), name), { free(name); free(buf); });

		//get data
		ERRCHECKDO(pop_buf(&msg->data, sizeof(size_t), &len), { free(name); free(buf); });
		if (len > buf_len)
		{
			REALLOCDO(buf, buf, (sizeof(char) * len), { free(name); free(buf); });
			buf_len = len;
		}
		ERRCHECKDO(pop_buf(&msg->data, (sizeof(char) * len), buf), { free(name); free(buf); });

		//write to file
		ERRCHECKDO(write_buf_to_file(name, buf, len, dirname), { free(name); free(buf); });
	}
	free(name);
	free(buf);
	return 0;
}

static int write_buf_to_file(char const* filename, char* buf, size_t filesize, const char* dirname)
{
	char* path = NULL;
	size_t flen = strlen(filename);
	size_t dlen = strlen(dirname);

	MALLOC(path, (flen + dlen + 1) * sizeof(char));
	strncpy(path, dirname, dlen);
	strncpy(path + dlen, filename, flen + 1);

	FILE* out = fopen(path, "wb");
	PTRCHECKDO(out, free(path));
	fwrite(buf, sizeof(char), filesize, out);
	fclose(out);

	free(path);
	return 0;
}