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
#define SRV_MSG(num) ((msg_t) num << 24 ) | MESSAGE_CLIENT_NULL
#define CNT_MSG(num) ((msg_t) num << 16 ) | MESSAGE_SERVER_NULL
#define FLAG(num) ((msg_t) 1 << num)

#define ISSERVER(msg) ((msg & MESSAGE_CLIENT_MASK) == MESSAGE_CLIENT_NULL)
#define GETSERVER(msg) (msg & MESSAGE_SERVER_MASK)
#define ISCLIENT(msg) ((msg & MESSAGE_SERVER_MASK) == MESSAGE_SERVER_NULL)
#define GETCLIENT(msg) (msg & MESSAGE_CLIENT_MASK)
#define GETFLAGS(msg) (msg & MESSAGE_FLAG_MASK)
#define HASFLAG(msg, flag) (msg & flag)

//server side messages
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
#define MESSAGE_FLAG_NONE FLAG(0)
#define MESSAGE_OPERATION_SUCCESS FLAG(1)
#define MESSAGE_OP_SUCC MESSAGE_OPERATION_SUCCESS
#define MESSAGE_FILE_EXISTS FLAG(2)
#define MESSAGE_FILE_NEXISTS FLAG(3)
#define MESSAGE_FILE_LOCK FLAG(4)
#define MESSAGE_FILE_NLOCK FLAG(5)
#define MESSAGE_FILE_NOWN FLAG(6)
#define MESSAGE_FILE_NPERM FLAG(7)
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

#endif
