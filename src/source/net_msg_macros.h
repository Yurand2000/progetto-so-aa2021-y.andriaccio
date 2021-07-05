#ifndef NETWORK_MESSAGE_MACROS
#define NETWORK_MESSAGE_MACROS

#include "errset.h"
#include "file.h"
#include "net_msg.h"
#include "message_type.h"

#define UNIX_PATH_MAX 108
#define FILE_PATH_MAX FILE_NAME_MAX_SIZE

/* all the funtions return 0 on success, -1 on failure setting errno. */

/* sends the message given by m_ptr to the socket. deletes the message afterwards.
 * calls return -1 in case of error and sets errno. */
#define SEND_TO_SOCKET(sock, m_ptr, func) {\
	ERRCHECKDO(write_msg(sock, m_ptr), { destroy_message(m_ptr); func; });\
}

/* reads to the message given by m_ptr from the socket. initializes the structure by itself.
 * calls return -1 in case of error, deletes the message and sets errno. */
#define READ_FROM_SOCKET(sock, m_ptr, func) {\
	ERRCHECKDO(read_msg(sock, m_ptr), { destroy_message(m_ptr); func; });\
}

/* sends the message given by m_ptr to the socket and reads (blocking)
 * a response message from the server. There are error checks which make the function
 * return -1 setting errno. It should clean the memory properly. */
#define SEND_RECEIVE_TO_SOCKET(sock, m_ptr, func) {\
	SEND_TO_SOCKET(sock, m_ptr, func);\
	destroy_message(m_ptr);\
	create_message(m_ptr);\
	READ_FROM_SOCKET(sock, m_ptr,func);\
}

int is_server_message(net_msg* message, msg_t message_num);
int is_client_message(net_msg* message, msg_t message_num);

#define CHECK_SERVER_MSG_TYPE(msg, type) ((msg & MESSAGE_SERVER_MASK) == (type))
#define CHECK_CLIENT_MSG_TYPE(msg, type) ((msg & MESSAGE_CLIENT_MASK) == (type))

#define BUILD_EMPTY_MESSAGE(m_ptr, tp) {\
	create_message(m_ptr);\
	(m_ptr)->type = tp;\
}

//control macros
#define SOCK_VALID(sock) if(sock < 0) ERRSET(ENOTCONN, -1);

#define SOCKNAME_VALID(sockname, len_ptr) {\
	*(len_ptr) = strlen(sockname) + 1;\
	if(*(len_ptr) > UNIX_PATH_MAX) ERRSET(ENAMETOOLONG, -1);\
}

#define FILENAME_VALID(file, len_ptr) {\
	*(len_ptr) = strlen(file) + 1;\
	if(*(len_ptr) > FILE_PATH_MAX) ERRSET(ENAMETOOLONG, -1);\
}

#endif
