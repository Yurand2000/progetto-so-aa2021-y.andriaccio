#include "worker_generics.h"

#include <errno.h>

#include "../file.h"
#include "../net_msg.h"
#include "../errset.h"

int get_file(file_t* files, size_t file_num, const char* filename)
{
	for (size_t i = 0; i < file_num; i++)
	{
		int val = is_existing_file(&files[i]);
		if (val == 0)
		{
			int chk = check_file_name(&files[i], filename);
			if (chk == 0) return i;
			else if (chk == -1) return -1;
		}
		else if (val == -1) return -1;
	}
	ERRSET(ENOENT, -1);
}

int get_empty_slot(file_t* files, size_t file_num)
{
	for (size_t i = 0; i < file_num; i++)
	{
		int val = is_existing_file(&files[i]);
		if (val == 1) return i;
		else if (val != 0) return -1;
	}
	ERRSET(EMFILE, -1);
}

int read_file_name(net_msg* msg, char* buf_name, size_t buf_size)
{
	size_t file_size;
	pop_buf(&msg->data, sizeof(size_t), file_size);
	if (file_size <= buf_size)
	{
		pop_buf(&msg->data, file_size * sizeof(char), buf_name);
		return 0;
	}
	else ERRSET(ENOBUFS, -1);
}



void do_log(log_t* log, int fileno, char* operation, char* file, char* error)
{
	char log_msg[MESSAGE_MAX_SIZE];
	snprintf(log_msg, MESSAGE_MAX_SIZE, "Client: %d; Op: %s; File: %s; %s",
	fileno, operation, file, error);
	print_line_to_log_file(log, log_msg);
}