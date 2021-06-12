#include "worker.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "errset.h"
#include "net_msg.h"
#include "message_type.h"
#include "net_msg_macros.h"

static int file_exists(file_t const* files, size_t file_num, const char* filename);
static int get_no_file(file_t const* files, size_t file_num);
static void read_file_name(net_msg* msg, char** file_name, size_t* file_size);

static int worker_do(int conn, file_t* files, size_t file_num, char* lastop_writefile_pname)
{
	//read message from connection	
	net_msg in_msg;
	READ_FROM_SOCKET(conn, &in_msg);
	msg_t flags = GETFLAGS(in_msg.type);

	if(!ISCLIENT(in_msg.type)) ERRSETDO(EBADMSG, destroy_message(&in_msg), -1);

	//switch based on request type
	net_msg out_msg;
	create_message(&out_msg);
	if(in_msg.type & MESSAGE_OPEN_CONN)
	{
		out_msg.type = MESSAGE_OCONN_ACK;
	}
	else if(in_msg.type & MESSAGE_CLOSE_CONN)
	{
		out_msg.type = MESSAGE_CCONN_ACK;
	}
	else if(in_msg.type & MESSAGE_OPEN_FILE)
	{
		size_t name_size; char* name;
		read_file_name(&in_msg, &name, &name_size);

		out_msg.type = MESSAGE_OFILE_ACK;

		int fex = file_exists(files, file_num, name);
		if (fex == -1 && errno != ENOENT)
		{
			SEND_TO_SOCKET(conn, &out_msg);
			destroy_message(&in_msg);
			return -1;
		}

		int owner = (flags & MESSAGE_OPEN_OLOCK) ? conn : OWNER_NULL;	//want to lock?
		if(flags & MESSAGE_OPEN_OCREATE)	//create file
		{
			if (fex == -1 && errno == ENOENT)
			{
				int nof = get_no_file(files, file_num);
				create_file_struct(&files[nof], name, owner);

				out_msg.type |= MESSAGE_OP_SUCC;

				if (flags & MESSAGE_OPEN_OLOCK)
					strncpy(lastop_writefile_pname, name, name_size);
			}
			else
				out_msg.type |= MESSAGE_FILE_EXISTS;
		}
		else	//open existing file
		{
			if (fex >= 0)
			{
				int opn = open_file(&files[fex], owner);
				if (opn == 0)
					out_msg.type |= MESSAGE_OP_SUCC;
				else
					out_msg.type |= MESSAGE_FILE_NPERM;
			}
			else
				out_msg.type |= MESSAGE_FILE_NEXISTS;
		}
	}
	else if(in_msg.type & MESSAGE_CLOSE_FILE)
	{
		size_t name_size; char* name;
		read_file_name(&in_msg, &name, &name_size);

		out_msg.type = MESSAGE_CFILE_ACK;

		int fex = file_exists(files, file_num, name);
		if (fex >= 0)
		{
			int lck = close_file(&files[fex], conn);
			if (lck == 0) out_msg.type |= MESSAGE_OP_SUCC;
			else if (errno == EPERM) out_msg.type |= MESSAGE_FILE_LOCK;
		}
		else if (errno == ENOENT) out_msg.type |= MESSAGE_FILE_NEXISTS;
	}
	else if(in_msg.type & MESSAGE_READ_FILE)
	{

	}
	else if (in_msg.type & MESSAGE_READN_FILE)
	{

	}
	else if(in_msg.type & MESSAGE_WRITE_FILE)
	{

	}
	else if(in_msg.type & MESSAGE_APPEND_FILE)
	{

	}
	else if(in_msg.type & MESSAGE_LOCK_FILE)
	{
		size_t name_size; char* name;
		read_file_name(&in_msg, &name, &name_size);

		out_msg.type = MESSAGE_LFILE_ACK;

		int fex = file_exists(files, file_num, name);
		if (fex >= 0)
		{
			int lck = lock_file(&files[fex], conn);
			if (lck == 0) out_msg.type |= MESSAGE_OP_SUCC;
			else if(errno == EPERM) out_msg.type |= MESSAGE_FILE_NOWN;
		}
		else if (errno == ENOENT) out_msg.type |= MESSAGE_FILE_NEXISTS;
	}
	else if(in_msg.type & MESSAGE_UNLOCK_FILE)
	{
		size_t name_size; char* name;
		read_file_name(&in_msg, &name, &name_size);

		out_msg.type = MESSAGE_ULFILE_ACK;
		int fex = file_exists(files, file_num, name);
		if (fex >= 0)
		{
			int lck = unlock_file(&files[fex], conn);
			if (lck == 0) out_msg.type |= MESSAGE_OP_SUCC;
			else if (errno == EPERM) out_msg.type |= MESSAGE_FILE_NOWN;
		}
		else if (errno == ENOENT) out_msg.type |= MESSAGE_FILE_NEXISTS;
	}
	else if(in_msg.type & MESSAGE_REMOVE_FILE)
	{
		size_t name_size; char* name;
		read_file_name(&in_msg, &name, &name_size);

		out_msg.type = MESSAGE_RMFILE_ACK;
		int fex = file_exists(files, file_num, name);
		if (fex >= 0)
		{
			if(files[fex].owner == OWNER_NULL)
				out_msg.type |= MESSAGE_FILE_NLOCK;
			else
			{
				int lck = remove_file(&files[fex], conn);
				if (lck == 0) out_msg.type |= MESSAGE_OP_SUCC;
				else if (errno == EPERM) out_msg.type |= MESSAGE_FILE_NOWN;
			}
		}
		else if (errno == ENOENT) out_msg.type |= MESSAGE_FILE_NEXISTS;
	}
	else
	{
		destroy_message(&out_msg);
		destroy_message(&in_msg);
		ERRSET(EBADMSG, -1);
	}

	SEND_TO_SOCKET(conn, &out_msg);
	destroy_message(&in_msg);
	return 0;
}

//returns the index of the matching file.
static int file_exists(file_t const* files, size_t file_num, const char* filename)
{
	for (size_t i = 0; i < file_num; i++)
	{
		int val = valid_file_struct(&files[i]);
		if (val == 0)
		{
			int chk = check_file_name(&files[i], filename);
			if (chk == 0) return i;
			else if (chk == -1) return -1;
		}
		else if (val == -1 && errno != ENOENT) return -1;
	}
	ERRSET(ENOENT, -1);
}

//returns the index of the first non existing file.
static int get_no_file(file_t const* files, size_t file_num)
{
	for (size_t i = 0; i < file_num; i++)
	{
		int val = valid_file_struct(&files[i]);
		if (val == -1 && errno == ENOENT) return i;
		else if (val != 0) return -1;
	}
	ERRSET(EMFILE, -1);
}

static void read_file_name(net_msg* msg, char** file_name, size_t* file_size)
{
	read_buf(&msg->data, sizeof(size_t), file_size);
	if (*file_size > FILENAME_MAX) *file_size = FILENAME_MAX;
	MALLOC(*file_name, *file_size);
	read_buf(&msg->data, *file_size * sizeof(char), *file_name);
}