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
#include "client_api_macros.h"

static int worker_do(int conn, file_t* files, size_t file_num, char* lastop_writefile_pname)
{
	//read message from connection	
	net_msg in_msg;
	create_message(&in_msg);
	READ_FROM_SOCKET(conn, &in_msg);

	if(!ISCLIENT(in_msg.type)) ERRSETDO(EBADMSG, destroy_message(&in_msg), -1);

	//switch based on request type
	net_msg out_msg;
	create_message(&out_msg);
	if(in_msg.type & MESSAGE_OPEN_CONN)
	{
		out_msg.type = MESSAGE_OCONN_ACK;
		SEND_TO_SOCKET(conn, out_msg);
	}
	else if(in_msg.type & MESSAGE_CLOSE_CONN)
	{
		out_msg.type = MESSAGE_CCONN_ACK;
		SEND_TO_SOCKET(conn, out_msg);
	}
	else if(in_msg.type & MESSAGE_OPEN_FILE)
	{
		msg_t flags = GETFLAGS(in_msg.type);
		if(flags & MESSAGE_OPEN_OCREATE)
		{

		}
		else
		{

		}
	}
	else if(in_msg.type & MESSAGE_CLOSE_FILE)
	{

	}
	else if(in_msg.type & MESSAGE_READ_FILE)
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

	}
	else if(in_msg.type & MESSAGE_UNLOCK_FILE)
	{

	}
	else if(in_msg.type & MESSAGE_REMOVE_FILE)
	{

	}
	else
	{
		destroy_message(&out_msg);
		ERRSETDO(EBADMSG, destroy_message(&in_msg), -1);
	}
	return 0;
}
