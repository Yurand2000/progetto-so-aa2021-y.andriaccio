#ifndef SERVER_DATA_MACROS
#define SERVER_DATA_MACROS

#define DEFAULT_CONFIG_FILE_NAME "./config.txt"
#define DEFAULT_SOCKET_FILE_NAME "./socket.sk"
#define DEFAULT_SOCKET_FILE_SIZE 12
#define DEFAULT_LOG_FILE_NAME "./log.txt"
#define DEFAULT_LOG_FILE_SIZE 10
#define FILE_NAME_MAX_SIZE 256
#define MAX_WORKER_THREADS 64
#define MAX_STORAGE_CAPACITY (long)(4 * 1024) * (long)(1024 * 1024)	//4GB
#define MAX_STORAGE_FILES 1024 * 1024
#define MAX_CONNECTIONS 50

typedef struct config_data
{
	char socket_file[FILE_NAME_MAX_SIZE];
	char log_file[FILE_NAME_MAX_SIZE];
	int worker_threads;
	long storage_capacity; //in bytes
	int max_files;
	int max_connections;
	char algorithm;
	int use_compression;
} cfg_t;

#endif