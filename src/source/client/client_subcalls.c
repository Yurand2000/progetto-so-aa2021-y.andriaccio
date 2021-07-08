#include "client_subcalls.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#include "../client_api.h"

void print_help()
{
#define HELP_TEXT "FileServer Client, help:\n"\
"Options marked as MANDATORY are mandatory to use (unless calling -h) while\n"\
"options marked as UNIQUE can be only specified once. If these conditions are\n"\
"not met, the program will terminate.\n"\
"Options:              | Description:\n"\
"---- Generic commands: -----------------------------------------\n"\
"-h                    - Show this help screen, then closes the program.\n"\
"-f                    - Socketfile to connect to. [MANDATORY, UNIQUE]\n"\
"-p                    - Print information for each given operation. [UNIQUE]\n"\
"-t time               - Time in milliseconds to wait between two requests to\n"\
"                        the server. If unspecified the time is set to 0.\n"\
"---- Write file commands: --------------------------------------\n"\
"-W file1[,file2]      - Send the given file[s] to the file server.\n"\
"-w dirname[,n=0]      - Send at most n (if n == 0 then all) files from the\n"\
"                        directory \"dirname\" to the server\n"\
"-a file1[,file2]      - Append a time stamp to the given file[s]."\
"[-D dirname]          - This option goes only with -w, -W or -a. Specifies a\n"\
"                        folder to write cachemiss files expelled from the\n"\
"                        server. It is optional.\n"\
"---- Read file commands: ---------------------------------------\n"\
"-r file1[,file2]      - Read the specified file[s] from the file server.\n"\
"                        -d option optional for this command.\n"\
"-R[n=0]               - Read any n (if n == 0 then all) from the file server.\n"\
"                        -d option is optional for this command.\n"\
"                        Note that the optional argument must have no spaces\n"\
"                        with the -R option, otherwise it will be skipped.\n"\
"-d dirname            - This option goes only with -r or -R. Specifies a folder\n"\
"                        to save read files. It is optional.\n"\
"---- File operation commands: ----------------------------------\n"\
"-l file1[,file2]      - Files to acquire mutual exclusion on its operations.\n"\
"-u file1[,file2]      - Files to release mutual exclusion on its operations.\n"\
"-c file1[,file2]      - Files to remove from the server if present.\n"\
"                        The operation requires mutual exclusion on the files.\n\n"

	printf(HELP_TEXT);
	fflush(stdout);
#undef HELP_TEXT
}

void print_operation_result(const char* op_type, const char* file, int res)
{
	char* op_res = (res != -1) ? "Success" : strerror(errno);

	int bytes_read, bytes_write;
	get_bytes_read_write(&bytes_read, &bytes_write);
	printf("Operation: %s; File: %s; Result: %s", op_type, file, op_res);
	if (res > 0)
		printf("; Files read: %d", res);
	if (bytes_read != 0 || bytes_write != 0)
		printf("; Bytes: read %d; write %d", bytes_read, bytes_write);
	printf(".\n");
}

int get_cwd(char currdir[], size_t* currdir_size)
{
	PTRCHECK(getcwd(currdir, FILENAME_MAX - 1));
	*currdir_size = strlen(*currdir);

	//add trailing / if necessary
	if (currdir[(*currdir_size) - 1] != '/')
	{
		currdir[(*currdir_size)] = '/';
		currdir[(*currdir_size) + 1] = '\0';
		(*currdir_size)++;
	}
	return 0;
}

int check_socket_file(char* socket_name)
{
	if (socket_name == NULL || socket_name[0] == '\0')
	{
		printf("-f flag is mandatory. start with -h for details.\n");
		ERRSET(ENOENT, -1);
	}
	else
	{
		ERRCHECKDO(access(socket_name, R_OK | W_OK),
			printf("socket file not found. Has the server started?\n"));
	}
	return 0;
}