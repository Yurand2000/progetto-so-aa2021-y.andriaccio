#ifndef NETWORK_MESSAGE_TYPE
#define NETWORK_MESSAGE_TYPE

#include <stdint.h>

#include "net_msg.h"

/* since the message type is a uint32_t, so it is exactly 32 bit on all platforms,
 * some of the bits are reserver for the message type, while others are for some flags.
 * |    8   |    8   |   8   |   8   | bits
 * | server | client | flags | flags |
 * there are in total 16 different flags assignable (their bitwise and is zero),
 * there are at most 256 different messages from the server, and 256 from the client.
 *
 * a server message will have all ones on the client part of the message,
 * a client message will have all zeroes on the server part of the message. */

typedef uint32_t msg_t;

#define MESSAGE_SERVER_NULL ((msg_t) 0 )
#define MESSAGE_CLIENT_NULL ((msg_t) (255 << 16) )
#define MESSAGE_SERVER_MASK ((msg_t) (255 << 24) )
#define MESSAGE_CLIENT_MASK ((msg_t) (255 << 16) )
#define MESSAGE_FLAG_MASK ((msg_t) ((255 << 8) + 255) )
#define SRV_MSG(num) (((msg_t) num << 24 ) | MESSAGE_CLIENT_NULL)
#define CNT_MSG(num) (((msg_t) num << 16 ) | MESSAGE_SERVER_NULL)
#define FLAG(num) ((msg_t) 1 << num)

#define ISSERVER(msg) ((msg & MESSAGE_CLIENT_MASK) == MESSAGE_CLIENT_NULL)
#define GETSERVER(msg) (msg & MESSAGE_SERVER_MASK)
#define ISCLIENT(msg) ((msg & MESSAGE_SERVER_MASK) == MESSAGE_SERVER_NULL)
#define GETCLIENT(msg) (msg & MESSAGE_CLIENT_MASK)
#define GETFLAGS(msg) (msg & MESSAGE_FLAG_MASK)
#define HASFLAG(msg, flag) (msg & flag)

//server side messages
#define MESSAGE_NULL SRV_MSG(0)
#define MESSAGE_OCONN_ACK SRV_MSG(1)
#define MESSAGE_CCONN_ACK SRV_MSG(2)
#define MESSAGE_OFILE_ACK SRV_MSG(3)
#define MESSAGE_CFILE_ACK SRV_MSG(4)
#define MESSAGE_RMFILE_ACK SRV_MSG(5)
#define MESSAGE_LFILE_ACK SRV_MSG(6)
#define MESSAGE_ULFILE_ACK SRV_MSG(7)
#define MESSAGE_RFILE_ACK SRV_MSG(8)
#define MESSAGE_RNFILE_ACK CNT_MSG(9)
#define MESSAGE_WFILE_ACK SRV_MSG(10)
#define MESSAGE_AFILE_ACK SRV_MSG(11)

//server side subcodes
#define MESSAGE_FLAG_NONE ((msg_t) 0);
#define MESSAGE_OPERATION_SUCCESS FLAG(0)
#define MESSAGE_OP_SUCC MESSAGE_OPERATION_SUCCESS
#define MESSAGE_FILE_EXISTS FLAG(2)
#define MESSAGE_FILE_NEXISTS FLAG(3)
#define MESSAGE_FILE_LOCK FLAG(4)
#define MESSAGE_FILE_NLOCK FLAG(5)
#define MESSAGE_FILE_NOWN FLAG(6)
#define MESSAGE_FILE_NPERM FLAG(7)
#define MESSAGE_FILE_EMFILE FLAG(8)
#define MESSAGE_FILE_CHACEMISS FLAG(15)

//client side messages
#define MESSAGE_OPEN_CONN CNT_MSG(1)
#define MESSAGE_CLOSE_CONN CNT_MSG(2)
#define MESSAGE_OPEN_FILE CNT_MSG(3)
#define MESSAGE_CLOSE_FILE CNT_MSG(4)
#define MESSAGE_READ_FILE CNT_MSG(5)
#define MESSAGE_READN_FILE CNT_MSG(6)
#define MESSAGE_WRITE_FILE CNT_MSG(7)
#define MESSAGE_APPEND_FILE CNT_MSG(8)
#define MESSAGE_LOCK_FILE CNT_MSG(9)
#define MESSAGE_UNLOCK_FILE CNT_MSG(10)
#define MESSAGE_REMOVE_FILE CNT_MSG(11)

//client side subcodes
#define MESSAGE_OPEN_OCREATE FLAG(1)
#define MESSAGE_OPEN_OLOCK FLAG(2)

//client request to string
#define STRING_OPEN_CONN "open connection"
#define STRING_CLOSE_CONN "close connection"
#define STRING_OPEN_FILE "open file"
#define STRING_CLOSE_FILE "close file"
#define STRING_READ_FILE "read file"
#define STRING_READN_FILE "read n files"
#define STRING_WRITE_FILE "write file"
#define STRING_APPEND_FILE "append to file"
#define STRING_LOCK_FILE "lock file"
#define STRING_UNLOCK_FILE "unlock file"
#define STRING_REMOVE_FILE "remove file"

//there are no checks on these macro functions.
#define GET_STRING_FROM_OPERATION(msg, out) {\
	if(msg & MESSAGE_OPEN_CONN) out = STRING_OPEN_CONN;\
	else if(msg & MESSAGE_CLOSE_CONN) out = STRING_CLOSE_CONN;\
	else if(msg & MESSAGE_OPEN_FILE) out = STRING_OPEN_FILE;\
	else if(msg & MESSAGE_CLOSE_FILE) out = STRING_CLOSE_FILE;\
	else if(msg & MESSAGE_READ_FILE) out = STRING_READ_FILE;\
	else if(msg & MESSAGE_READN_FILE) out = STRING_READN_FILE;\
	else if(msg & MESSAGE_WRITE_FILE) out = STRING_WRITE_FILE;\
	else if(msg & MESSAGE_APPEND_FILE) out = STRING_APPEND_FILE;\
	else if(msg & MESSAGE_LOCK_FILE) out = STRING_LOCK_FILE;\
	else if(msg & MESSAGE_UNLOCK_FILE) out = STRING_UNLOCK_FILE;\
	else if(msg & MESSAGE_REMOVE_FILE) out = STRING_REMOVE_FILE;\
}

#define GET_STRING_FROM_FLAGS(msg, out)  {\
	if(msg & MESSAGE_OPEN_OCREATE) out = STRING_OPEN_OCREATE;\
	else if(msg & MESSAGE_OPEN_OLOCK) out = STRING_OPEN_OLOCK;\
}

#endif
