#include "client_api.h"
#include "win32defs.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "errset.h"
#include "net_msg.h"
#include "message_type.h"
#include "net_msg_macros.h"

//static variables
/* the connection file (socket) is stored as a static variable hidden into this file. */
static int conn = -1;

static int write_cached_file(net_msg* msg, char* buf, size_t buf_size, const char* dirname);

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

	msg_t flg = MESSAGE_FLAG_NONE;
	if(flags & O_CREATE) flg |= MESSAGE_OPEN_OCREATE;
	if(flags & O_LOCK) flg |= MESSAGE_OPEN_OLOCK;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_OPEN_FILE | flg);
	write_buf(&msg.data, len, pathname);
	write_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg);

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
	write_buf(&msg.data, len, pathname);
	write_buf(&msg.data, sizeof(size_t), &len);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg);

	if(CHECK_MSG_CNT(&msg, MESSAGE_RFILE_ACK))
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
		MALLOCDO(*buf, (sizeof(char) * (*size)), destroy_message(&msg));
		ERRCHECKDO(read_buf(&msg.data, *size, *buf),
			{ destroy_message(&msg); free(*buf); } );
		destroy_message(&msg);
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
	ERRCHECK(fstat(file, &file_stats));
	size_t file_size = file_stats.st_size;
	size_t block_size = file_stats.st_blksize; //block size for efficient I/O, as stated by the manual.

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_WRITE_FILE);

	//now write the file into the message
	char* buf; MALLOCDO(buf, (sizeof(char) * block_size), destroy_message(&msg));
	ssize_t read_size;
	while((read_size = read(file, buf, block_size)) > 0)
		write_buf(&msg.data, read_size, buf);
	if(read_size == -1)
	{
		destroy_message(&msg);
		free(buf);
		return -1;
	}
	write_buf(&msg.data, sizeof(size_t), &file_size);
	write_buf(&msg.data, len, pathname);
	write_buf(&msg.data, sizeof(size_t), &len);

	//if last read was 0, then it should have succeded into creating the message
	close(file);
	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg);

	int cachemiss = 0;
	if(CHECK_MSG_CNT(&msg, MESSAGE_WFILE_ACK))
	{

		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_FILE_CHACEMISS))
			cachemiss = 1;

		if(HASFLAG(flags, MESSAGE_OP_SUCC))
			;
		else if(HASFLAG(flags, MESSAGE_FILE_NOWN))
			{ ERRSETDO(EPERM, destroy_message(&msg), -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS))
			{ ERRSETDO(ENOENT, destroy_message(&msg), -1); }
		else
			{ ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
	}
	else { ERRSETDO(EBADMSG, destroy_message(&msg), -1); }

	if(cachemiss == 1)
	{
		ERRCHECKDO(write_cached_file(&msg, buf, block_size, dirname),
			{ destroy_message(&msg); free(buf); } );
	}
	
	destroy_message(&msg);
	free(buf);
	return 0;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_APPEND_FILE);

	//write the appended data into the message
	write_buf(&msg.data, size, buf);
	write_buf(&msg.data, sizeof(size_t), &size);
	write_buf(&msg.data, len, pathname);
	write_buf(&msg.data, sizeof(size_t), &len);

	set_checksum(&msg);

	SEND_RECEIVE_TO_SOCKET(conn, &msg, &msg);

	int cachemiss = 0;
	if(CHECK_MSG_CNT(&msg, MESSAGE_AFILE_ACK))
	{
		msg_t flags = GETFLAGS(msg.type);
		if(HASFLAG(flags, MESSAGE_FILE_CHACEMISS))
			cachemiss = 1;

		if(HASFLAG(flags, MESSAGE_OP_SUCC))
			;
		else if(HASFLAG(flags, MESSAGE_FILE_NOWN))
			{ ERRSETDO(EPERM, destroy_message(&msg), -1); }
		else if(HASFLAG(flags, MESSAGE_FILE_NEXISTS))
			{ ERRSETDO(ENOENT, destroy_message(&msg), -1); }
		else
			{ ERRSETDO(EBADMSG, destroy_message(&msg), -1); }
	}
	else { ERRSETDO(EBADMSG, destroy_message(&msg), -1); }

	if(cachemiss == 1)
	{
		size_t block_size = 512; //hardcode constant
		char* block; MALLOCDO(block, (sizeof(char) * block_size), destroy_message(&msg));

		ERRCHECKDO(write_cached_file(&msg, block, block_size, dirname),
			{ destroy_message(&msg); free(block); } );
		free(block);
	}

	destroy_message(&msg);
	return 0;
}

int lockFile(const char* pathname)
{
	SOCK_VALID(conn);
	size_t len; FILENAME_VALID(pathname, &len); len++;

	net_msg msg;
	BUILD_EMPTY_MESSAGE(&msg, MESSAGE_LOCK_FILE);
	write_buf(&msg.data, len, pathname);
	write_buf(&msg.data, sizeof(size_t), &len);
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
	write_buf(&msg.data, len, pathname);
	write_buf(&msg.data, sizeof(size_t), &len);
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
	write_buf(&msg.data, len, pathname);
	write_buf(&msg.data, sizeof(size_t), &len);
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
	write_buf(&msg.data, len, pathname);
	write_buf(&msg.data, sizeof(size_t), &len);
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

static int write_cached_file(net_msg* msg, char* buf, size_t buf_size, const char* dirname)
{
	//get expelled file name
	size_t ex_file_len;
	read_buf(&msg->data, sizeof(size_t), &ex_file_len);
	char* ex_file_name;
	MALLOC(ex_file_name, (sizeof(char) * ex_file_len));
	read_buf(&msg->data, ex_file_len, ex_file_name);

	//merge dirname with expelled file name
	size_t dir_len = strlen(dirname);
	char* dir_file_name;
	MALLOCDO(dir_file_name, (sizeof(char) * (dir_len + ex_file_len + 1)),
		free(ex_file_name) );

	strncpy(dir_file_name, dirname, dir_len);
	dir_file_name[dir_len] = '/';
	strncpy(dir_file_name + dir_len + 1, ex_file_name, ex_file_len);
	free(ex_file_name);

	//open expelled file
	int ex_file; ERRCHECKDO((ex_file = open(dir_file_name, O_CREAT | O_EXCL)),
		free(dir_file_name));
	free(dir_file_name);

	//fill expelled file
	int read_check;
	size_t ex_file_size; size_t r_size = buf_size; size_t offset = 0;
	read_buf(&msg->data, sizeof(size_t), &ex_file_size);
	while((read_check = read_buf(&msg->data, r_size, buf)) == 0 &&
		offset < ex_file_size)
	{
		write(ex_file_size, buf, r_size);
		offset += r_size;
		if(ex_file_size - offset < buf_size)
			r_size = ex_file_size - offset;
	}

	close(ex_file);
	if(offset < ex_file_size)
		return -1;
	else
		return 0;
}
