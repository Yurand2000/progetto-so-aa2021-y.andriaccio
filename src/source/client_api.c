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
static int byte_read = 0, byte_write = 0;

static int save_cached_files(net_msg* msg, const char* dirname);

static int write_buf_to_file(char const* filename, char* buf, size_t filesize, const char* dirname);

static void clear_byte_read_write();

#define CLOSE { close(conn); conn = -1; }

int openConnection(const char* sockname, int msec, const struct timespec abstime)
{
	clear_byte_read_write();
	if (conn >= 0) ERRSET(EISCONN, -1);

	size_t len; SOCKNAME_VALID(sockname, &len);
	conn = socket(AF_UNIX, SOCK_STREAM, 0);
	if(conn == -1) return -1;

	//generate connection address
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockname, len + 1);

	//timespec struct to wait
	struct timespec wait_timer;
	long nsec = (long)msec * 1000000L;
	wait_timer.tv_sec = nsec / 1000000000L;
	wait_timer.tv_nsec = nsec % 1000000000L;

	long tot_time, abs_time;
	tot_time = 0;
	abs_time = (long)abstime.tv_sec * 1000000000L + abstime.tv_nsec;

	while(connect(conn, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) == -1)
	{
		if(errno == ENOENT || errno == ECONNREFUSED)
		{
			//check if you are out of time
			if(tot_time > abs_time)
				ERRSETDO(ETIMEDOUT, CLOSE, -1);

			//retry wait time
			ERRCHECKDO(nanosleep(&wait_timer, NULL), { CLOSE; return -1; });
			tot_time += (long)(wait_timer.tv_sec) * 1000000000L + wait_timer.tv_nsec;
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

	SEND_RECEIVE_TO_SOCKET(conn, &msg, CLOSE);

	int check = is_server_message(&msg, MESSAGE_OCONN_ACK);
	destroy_message(&msg);
	if(check == 0)
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
	clear_byte_read_write();
	SOCK_VALID(conn);

	//send close message, just to be polite with the server.
	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_CLOSE_CONN);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, CLOSE);
	destroy_message(&msg);
	//we wait for the server answer just to be sure it has received our message.

	return close(conn);
}

int openFile(const char* pathname, int flags)
{
	clear_byte_read_write();
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

	SEND_RECEIVE_TO_SOCKET(conn, &msg, CLOSE);

	int check = is_server_message(&msg, MESSAGE_OFILE_ACK);
	destroy_message(&msg);
	if(check == 0)
	{
		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_OP_SUCC))
			return 0;
		else if(HASFLAG(flags, MESSAGE_FILE_NOWN))
			{ ERRSET(EPERM, -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_EXISTS))
			{ ERRSET(EEXIST, -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS))
			{ ERRSET(ENOENT, -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_ERRMAXFILES))
			{ ERRSET(ENFILE, -1); }
		else
			{ ERRSET(EBADMSG, -1); }
	}
	else { ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
}

int readFile(const char* pathname, void** buf, size_t* size)
{
	clear_byte_read_write();
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_READ_FILE);
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, CLOSE);

	if(is_server_message(&msg, MESSAGE_RFILE_ACK) == 0)
	{ 
		msg_t flags = GETFLAGS(msg.type);
		if (HASFLAG(flags, MESSAGE_OP_SUCC))
		{
			ERRCHECKDO(pop_buf(&msg.data, sizeof(size_t), size), destroy_message(&msg));
			MALLOCDO(*buf, (sizeof(char) * (*size)), destroy_message(&msg));
			ERRCHECKDO(pop_buf(&msg.data, *size, *buf), { destroy_message(&msg); free(*buf); });

			byte_read += *size;
			destroy_message(&msg);
		}
		else if(HASFLAG(flags, MESSAGE_FILE_NOWN) || HASFLAG(flags, MESSAGE_FILE_NOPEN))
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
	clear_byte_read_write();
	SOCK_VALID(conn);

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_READN_FILE);
	push_buf(&msg.data, sizeof(int), &n);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, CLOSE);

	if (is_server_message(&msg, MESSAGE_RNFILE_ACK) == 0)
	{
		msg_t flags = GETFLAGS(msg.type);
		if (HASFLAG(flags, MESSAGE_OP_SUCC))
		{
			if(dirname != NULL)
				ERRCHECKDO(save_cached_files(&msg, dirname), { destroy_message(&msg); });
		}
		else
		{ ERRSETDO(EBADMSG, destroy_message(&msg), -1); }

		return 0;
	}
	else { ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
}

int writeFile(const char* pathname, const char* dirname)
{
	clear_byte_read_write();
	SOCK_VALID(conn);
	size_t len;
	
	if(dirname != NULL)
		FILENAME_VALID(dirname, &len);
	FILENAME_VALID(pathname, &len);

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
	byte_write += file_size;

	free(buf);
	close(file);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, CLOSE);

	int cachemiss = 0;
	if(is_server_message(&msg, MESSAGE_WFILE_ACK) == 0)
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
		else if(HASFLAG(flags, MESSAGE_FILE_NLOCK))
			{ ERRSETDO(EPERM, destroy_message(&msg), -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_EXISTS))
			{ ERRSETDO(EEXIST, destroy_message(&msg), -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS) || HASFLAG(flags, MESSAGE_FILE_NOPEN))
			{ ERRSETDO(ENOENT, destroy_message(&msg), -1); }
		else
			{ ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
	}
	else { ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
	clear_byte_read_write();
	SOCK_VALID(conn);
	size_t len; if (dirname != NULL) FILENAME_VALID(dirname, &len);
	FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_APPEND_FILE);

	//write the appended data into the message
	push_buf(&msg.data, size, buf);
	push_buf(&msg.data, sizeof(size_t), &size);
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);
	byte_write += size;

	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, CLOSE);

	int cachemiss = 0;
	if(is_server_message(&msg, MESSAGE_AFILE_ACK) == 0)
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
		else if(HASFLAG(flags, MESSAGE_FILE_NLOCK))
			{ ERRSETDO(EPERM, destroy_message(&msg), -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS) || HASFLAG(flags, MESSAGE_FILE_NOPEN))
			{ ERRSETDO(ENOENT, destroy_message(&msg), -1); }
		else
			{ ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
	}
	else { ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
}

int lockFile(const char* pathname)
{
	clear_byte_read_write();
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg, srv;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_LOCK_FILE);
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

#define WAIT_TIMER 200L * 1000000L //200 milliseconds
	int exit = 0;
	while (!exit) //loop until the lock is acquired
	{
		SEND_TO_SOCKET(conn, &msg, CLOSE);

		create_message(&srv);
		READ_FROM_SOCKET(conn, &srv, CLOSE);
		int check = is_server_message(&srv, MESSAGE_LFILE_ACK);
		msg_t flags = GETFLAGS(srv.type);
		destroy_message(&srv);
		if(check == 0)
		{
			if (HASFLAG(flags, MESSAGE_OP_SUCC))
				exit = 1;
			else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS) || HASFLAG(flags, MESSAGE_FILE_NOPEN))
				{ ERRSET(ENOENT, -1); }
			else if (HASFLAG(flags, MESSAGE_FILE_NOWN))
			{
				struct timespec wait;
				wait.tv_sec = 0;
				wait.tv_nsec = WAIT_TIMER;
				ERRCHECK(nanosleep(&wait, NULL));
			}
			else
				{ ERRSET(EBADMSG, -1); }
		}
		else { ERRSET(EBADMSG, -1); }
	}
#undef WAIT_TIMER
	destroy_message(&msg);
	return 0;
}

int unlockFile(const char* pathname)
{
	clear_byte_read_write();
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_UNLOCK_FILE);
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, CLOSE);

	int check = is_server_message(&msg, MESSAGE_ULFILE_ACK);
	destroy_message(&msg);
	if(check == 0)
	{
		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_OP_SUCC))
			return 0;
		else if(HASFLAG(flags, MESSAGE_FILE_NOWN))
			{ ERRSET(EPERM, -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS) || HASFLAG(flags, MESSAGE_FILE_NOPEN))
			{ ERRSET(ENOENT, -1); }
		else
			{ ERRSET(EBADMSG, -1); }
	}
	else { ERRSET(EBADMSG, -1); }
}

int closeFile(const char* pathname)
{
	clear_byte_read_write();
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_CLOSE_FILE);
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, CLOSE);

	int check = is_server_message(&msg, MESSAGE_CFILE_ACK);
	destroy_message(&msg);
	if(check == 0)
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

int removeFile(const char* pathname)
{
	clear_byte_read_write();
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_REMOVE_FILE);
	push_buf(&msg.data, len, pathname);
	push_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, CLOSE);

	int check = is_server_message(&msg, MESSAGE_RMFILE_ACK);
	destroy_message(&msg);
	if(check == 0)
	{
		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_OP_SUCC))
			return 0;
		else if(HASFLAG(flags, MESSAGE_FILE_NOWN) || HASFLAG(flags, MESSAGE_FILE_NLOCK))
			{ ERRSET(EPERM, -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS) || HASFLAG(flags, MESSAGE_FILE_NOPEN))
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
	printf("Debug ------- Count: %ld\n", count);
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
		byte_read += len;

		//write to file
		ERRCHECKDO(write_buf_to_file(name, buf, len, dirname), { free(name); free(buf); });
		printf("Debug ------- File: %s\n", name);
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

static void clear_byte_read_write()
{
	byte_read = byte_write = 0;
}

int get_bytes_read_write(int* read, int* write)
{
	if (read == NULL || write == NULL) ERRSET(EINVAL, -1);
	*read = byte_read;
	*write = byte_write;
	return 0;
}