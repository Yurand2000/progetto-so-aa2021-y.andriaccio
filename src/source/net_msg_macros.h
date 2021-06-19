#ifndef NETWORK_MESSAGE_MACROS
#define NETWORK_MESSAGE_MACROS

#include "errset.h"
#include "file.h"
#include "net_msg.h"
#include "message_type.h"

#define UNIX_PATH_MAX 108
#define FILE_PATH_MAX FILE_NAME_MAX_SIZE

/* sends the message given by m_ptr to the socket. deletes the message afterwards.
 * calls return -1 in case of error and sets errno. */
#define SEND_TO_SOCKET(sock, m_ptr) {\
	ERRCHECKDO(write_msg(sock, m_ptr), destroy_message(m_ptr));\
	destroy_message(m_ptr);\
}

/* reads to the message given by m_ptr from the socket. initializes the structure by itself.
 * calls return -1 in case of error, deletes the message and sets errno. */
#define READ_FROM_SOCKET(sock, m_ptr) {\
	create_message(m_ptr);\
	ERRCHECKDO(read_msg(sock, m_ptr), destroy_message(m_ptr));\
}

/* sends the message given by m_out_ptr to the socket and reads (blocking)
 * a response message from the server. There are error checks which make the function
 * return -1 setting errno. It should clean the memory properly. */
#define SEND_RECEIVE_TO_SOCKET(sock, m_out_ptr, m_in_ptr) {\
	SEND_TO_SOCKET(sock, m_out_ptr);\
	READ_FROM_SOCKET(sock, m_in_ptr);\
}

#define CHECK_MSG_SRV(m_ptr, tp) (check_checksum(m_ptr) &&\
	ISSERVER((m_ptr)->type) &&\
	( GETSERVER((m_ptr)->type) == (tp) ) )

#define CHECK_MSG_CNT(m_ptr, tp) (check_checksum(m_ptr) &&\
	ISCLIENT((m_ptr)->type) &&\
	( GETCLIENT((m_ptr)->type) == (tp) ) )

#define BUILD_EMPTY_MESSAGE(m_ptr, tp) {\
	create_message(m_ptr);\
	(m_ptr)->type = tp;\
}

//control macros
#define SOCK_VALID(sock) if(sock < 0) ERRSET(ENOTCONN, -1);

#define SOCKNAME_VALID(sockname, len_ptr) {\
	*(len_ptr) = strlen(sockname);\
	if(*(len_ptr) > UNIX_PATH_MAX) ERRSET(ENAMETOOLONG, -1);\
}

#define FILENAME_VALID(file, len_ptr) {\
	*(len_ptr) = strlen(file);\
	if(*(len_ptr) > FILE_PATH_MAX) ERRSET(ENAMETOOLONG, -1);\
}

#endif
