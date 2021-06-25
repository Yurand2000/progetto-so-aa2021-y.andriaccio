#include "net_msg_macros.h"

int is_server_message(net_msg* message, msg_t message_num)
{
	int ret = check_checksum(message);
	if (ret == -1) return -1;
	if (!ret &&
		ISSERVER(message->type) &&
		(GETSERVER(message->type) == (message_num & MESSAGE_SERVER_MASK)))
		return 0;
	else
		return 1;
}

int is_client_message(net_msg* message, msg_t message_num)
{
	int ret = check_checksum(message);
	if (ret == -1) return -1;
	if (!ret &&
		ISCLIENT(message->type) &&
		(GETCLIENT(message->type) == (message_num & MESSAGE_CLIENT_MASK)))
		return 0;
	else
		return 1;
}