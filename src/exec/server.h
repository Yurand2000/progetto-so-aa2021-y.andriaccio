#ifndef SERVER_DATA_MACROS
#define SERVER_DATA_MACROS

#define DEFAULT_CONFIG_FILE_NAME "./config.txt"
#define DEFAULT_SOCKET_FILE_NAME "./socket.sk"
#define DEFAULT_SOCKET_FILE_SIZE sizeof(DEFAULT_SOCKET_FILE_NAME)
#define DEFAULT_LOG_FILE_NAME "./log.txt"
#define DEFAULT_LOG_FILE_SIZE sizeof(DEFAULT_SOCKET_FILE_NAME)
#define FILENAME_MAX_SIZE 256
#define MAX_WORKER_THREADS 64
#define MAX_STORAGE_CAPACITY 4 * 1024 * 1024 * 1024	//4GB
#define MAX_STORAGE_FILES 1024 * 1024
#define MAX_CONNECTIONS 50

typedef struct config_data
{
	char socket_file[FILENAME_MAX_SIZE];
	char log_file[FILENAME_MAX_SIZE];
	int worker_threads;
	long storage_capacity; //in bytes
	int max_files;
	int max_connections;
	char algorithm;
} cfg_t;

void command_line_parsing(int argc, char* argv[], cfg_t* config_data);

void init_default_config(cfg_t* cfg);

void parse_config_from_file(cfg_t* cfg, char const* filename);

#endif