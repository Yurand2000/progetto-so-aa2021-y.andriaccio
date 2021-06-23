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

int prepare_socket(int* sock, cfg_t* config)
{
	size_t len; SOCKNAME_VALID(config->socket_file, &len);
	unlink(config->socket_file);
	ERRCHECK((*sock = socket(AF_UNIX, SOCK_STREAM, 0)));

	//generate connection address
	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, config->socket_file, UNIX_PATH_MAX);

	//bind to listen
	ERRCHECK(bind(*sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)));
	ERRCHECK(listen(*sock, config->max_connections));
	return 0;
}

int signal_to_file(int* signal_fd, sigset_t* oldmask)
{
	sigset_t sig_mask;
	ERRCHECK(sigemptyset(&sig_mask));
	ERRCHECK(sigaddset(&sig_mask, SIGINT));
	ERRCHECK(sigaddset(&sig_mask, SIGQUIT));
	ERRCHECK(sigaddset(&sig_mask, SIGHUP));
	ERRCHECK(sigaddset(&sig_mask, SIGUSR1)); //triggered when thread finish
											 //ignore signals, those are delivered only to the signal file descriptor.
	ERRCHECK(pthread_sigmask(SIG_SETMASK, &sig_mask, oldmask));
	ERRCHECK( (*signal_fd = signalfd(-1, &sig_mask, 0)) ); //create file descriptor
	return 0;
}

int initialize_poll_array(int socket, int signal_fd, struct pollfd** poll_array,
	nfds_t* poll_size, cfg_t* config)
{
	*poll_size = config->max_connections + 2;
	MALLOC(*poll_array, *poll_size * sizeof(struct pollfd));

	//fill poll array
	for (size_t i = 0; i < config->max_connections; i++)
	{
		(*poll_array)[i].fd = -1;
		(*poll_array)[i].events = POLLIN;
	}
	(*poll_array)[*poll_size - 2].fd = socket; //listener
	(*poll_array)[*poll_size - 2].events = POLLIN;
	(*poll_array)[*poll_size - 1].fd = signal_fd; //signal listener
	(*poll_array)[*poll_size - 1].events = POLLIN;
	return 0;
}