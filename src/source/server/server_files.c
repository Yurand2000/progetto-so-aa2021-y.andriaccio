#include "server_subcalls.h"
#include "../win32defs.h"

#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/signalfd.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../errset.h"
#include "../net_msg_macros.h"
#include "../config.h"
#include "../log.h"
#include "../worker.h"
#include "../readn_writen.h"

int initialize_file_structure(file_t** files, size_t* file_num, cfg_t* config)
{
	*file_num = config->max_files;
	MALLOC(*files, sizeof(file_t) * *file_num);
	for (size_t i = 0; i < *file_num; i++)
		init_file_struct(&(*files)[i]);
	return 0;
}