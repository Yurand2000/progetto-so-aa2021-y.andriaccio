#ifndef WIN32DEFS
#define WIN32DEFS

#ifdef WIN32

typedef int ssize_t;

//pthread defines
typedef int pthread_mutex_t;
typedef int pthread_cond_t;
typedef int pthread_t;
typedef int pthread_attr_t;

int pthread_mutex_init(pthread_mutex_t* mux, void*);
int pthread_mutex_destroy(pthread_mutex_t* mux);
int pthread_mutex_lock(pthread_mutex_t* mux);
int pthread_mutex_unlock(pthread_mutex_t* mux);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_create(pthread_t *th, const pthread_attr_t *attr, void *(*routine)(void*), void *args);

//socket defines
#define AF_UNIX 0
#define SOCK_STREAM 0

struct sockaddr { int a; };
struct sockaddr_un { int sun_family; char sun_path[108]; };
typedef int socklen_t;

int socket(int domain, int type, int protocol);
int bind(int sock_fd, const struct sockaddr* sa, socklen_t sa_len);
int listen(int sock_fd, int backlog);

//poll defines
#define POLLIN 0

typedef int nfds_t;

struct signalfd_siginfo {
	uint32_t ssi_signo;    /* Signal number */
	int32_t  ssi_errno;    /* Error number (unused) */
	int32_t  ssi_code;     /* Signal code */
	uint32_t ssi_pid;      /* PID of sender */
	uint32_t ssi_uid;      /* Real UID of sender */
	int32_t  ssi_fd;       /* File descriptor (SIGIO) */
	uint32_t ssi_tid;      /* Kernel timer ID (POSIX timers)
						   uint32_t ssi_band;     /* Band event (SIGIO) */
	uint32_t ssi_overrun;  /* POSIX timer overrun count */
	uint32_t ssi_trapno;   /* Trap number that caused signal */
	int32_t  ssi_status;   /* Exit status or signal (SIGCHLD) */
	int32_t  ssi_int;      /* Integer sent by sigqueue(3) */
	uint64_t ssi_ptr;      /* Pointer sent by sigqueue(3) */
	uint64_t ssi_utime;    /* User CPU time consumed (SIGCHLD) */
	uint64_t ssi_stime;    /* System CPU time consumed
						   (SIGCHLD) */
	uint64_t ssi_addr;     /* Address that generated signal
						   (for hardware-generated signals) */
	uint16_t ssi_addr_lsb; /* Least significant bit of address
						   (SIGBUS; since Linux 2.6.37)
						   uint8_t  pad[X];       /* Pad size to 128 bytes (allow for
						   additional fields in the future) */
};

struct pollfd { int fd; int events; int revents; };

int poll(struct pollfd fds[], nfds_t nfds, int timeout);

//signal file descriptor
#define SIGINT 0
#define SIGQUIT 0
#define SIGHUP 0
#define SIGUSR1 0

#define SIG_SETMASK 0

typedef int sigset_t;
int sigemptyset(sigset_t* pset);
int sigfillset(sigset_t* pset);
int sigaddset(sigset_t* pset, int signum);
int sigdelset(sigset_t* pset, int signum);
int sigismember(const sigset_t* pset, int signum);
int pthread_sigmask(int how, const sigset_t *set, sigset_t * oset);
int signalfd(int fd, const sigset_t *mask, int flags);

#endif

#endif