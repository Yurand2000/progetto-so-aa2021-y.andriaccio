#include "worker.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#include "errset.h"
#include "net_msg.h"
#include "message_type.h"
#include "net_msg_macros.h"

#define ERRCK(cond) if(cond == -1) return (void*)1;

static int file_exists(file_t* files, size_t file_num, const char* filename);
static int get_no_file(file_t* files, size_t file_num);
static int read_file_name(net_msg* msg, char** file_name, size_t* file_size);
static int worker_do(int* conn, int* newconn, file_t* files, size_t file_num,
	log_t* log, char* lastop_writefile_pname);

static int evict_FIFO(file_t* files, size_t file_num, size_t* last_evicted,
	long curr_st, int curr_f, pthread_mutex_t* state_mux);
static int evict_LRU(file_t* files, size_t file_num, size_t* last_evicted,
	long curr_st, int curr_f, pthread_mutex_t* state_mux);
static int evict_LFU(file_t* files, size_t file_num, size_t* last_evicted,
	long curr_st, int curr_f, pthread_mutex_t* state_mux);

void* worker_routine(void* args)
{
	worker_data* data = (worker_data*)args;
	while(1)
	{
		ERRCK(pthread_mutex_lock(&data->thread_mux));
		while (data->do_work != WORKER_DO)
			ERRCK(pthread_cond_wait(&data->thread_cond, &data->thread_mux));

		if (data->exit)
		{
			ERRCK(pthread_mutex_unlock(&data->thread_mux));
			return (void*)0;
		}
		else
		{
			int conn = data->work_conn;
			int newconn = data->new_conn;
			char* conn_op = data->work_conn_lastop;
			ERRCK(pthread_mutex_unlock(&data->thread_mux));
			ERRCK(worker_do(&conn, &newconn, data->files, data->file_num, data->log, conn_op));

			ERRCK(pthread_mutex_lock(&data->thread_mux));
			data->work_conn = conn;
			data->new_conn = newconn;
			data->do_work = WORKER_DONE;
			ERRCK(pthread_mutex_unlock(&data->thread_mux));
			ERRCK(raise(SIGUSR1));	//tell the main thread it has finished working
		}
	}
}

/* this is an internal function used by the worker to handle a request.
 * when returning, the values of conn and newconn tell the main thread if a
 * connection has been added or dropped.
 * lastop_writefile_pname contains the pathname of the file if the last operation
 * was an open with O_CREATE and O_LOCK flags and had success. */
static int worker_do(int* conn, int* newconn, file_t* files, size_t file_num, log_t* log, char* lastop_writefile_pname)
{
	net_msg in_msg, out_msg; char log_msg[FILENAME_MAX_SIZE];

	if (*conn == *newconn)	//this is the acceptor
	{
		ERRCHECK((*newconn = accept(*conn, NULL, 0)));

		//waiting for a message from the new connection, otherwise drop it.
		//need a timeout!!
		READ_FROM_SOCKET(*conn, &in_msg);
		msg_t flags = GETFLAGS(in_msg.type);

		//OPEN CONNECTION -----------------------------------------------------
		if (ISCLIENT(in_msg.type) && (in_msg.type & MESSAGE_OPEN_CONN))
		{
			out_msg.type = MESSAGE_OCONN_ACK;

			//logging
			snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s", *newconn, STRING_OPEN_CONN);
			print_line_to_log_file(log, log_msg);
			return 0;
		}
		else
		{
			*newconn = -1;

			snprintf(log_msg, FILENAME_MAX_SIZE, "Unknown client tried to connect.");
			print_line_to_log_file(log, log_msg);
			return 0;
		}
	}

	//read message from connection
	READ_FROM_SOCKET(*conn, &in_msg);
	msg_t flags = GETFLAGS(in_msg.type);

	if(!ISCLIENT(in_msg.type)) ERRSETDO(EBADMSG, destroy_message(&in_msg), -1);

	//switch based on request type
	create_message(&out_msg);

	//CLOSE CONNECTION --------------------------------------------------------
	if(in_msg.type & MESSAGE_CLOSE_CONN)
	{
		out_msg.type = MESSAGE_CCONN_ACK;

		//unown any locked files and close them
		/*for (size_t i = 0; i < file_num; i++)
		{
			if (is_locked_file(&files[i], *conn) == 0)
				close_file(&files[i], *conn, );
		}*/

		//close the connection
		close(*conn);
		*conn = -1;

		//logging
		snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s", *conn, STRING_CLOSE_CONN);
		print_line_to_log_file(log, log_msg);
	}

	//OPEN FILE ---------------------------------------------------------------
	else if(in_msg.type & MESSAGE_OPEN_FILE)
	{
		size_t name_size; char* name;
		ERRCHECK(read_file_name(&in_msg, &name, &name_size));

		out_msg.type = MESSAGE_OFILE_ACK;

		int fex = file_exists(files, file_num, name);
		if (fex == -1 && errno != ENOENT)
		{
			//file error!
			SEND_TO_SOCKET(*conn, &out_msg);
			destroy_message(&in_msg);

			//logging
			snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
				"File error.", *conn, STRING_OPEN_FILE, name);
			print_line_to_log_file(log, log_msg);
			return -1;
		}

		int owner = (flags & MESSAGE_OPEN_OLOCK) ? *conn : OWNER_NULL;	//want to lock?
		if(flags & MESSAGE_OPEN_OCREATE)	//create file
		{
			if (fex == -1 && errno == ENOENT)
			{
				int nof = get_no_file(files, file_num);
				if (nof == -1)
				{
					//max files in memory!
					out_msg.type |= MESSAGE_FILE_EMFILE;

					//logging
					snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
						"Too many open files.", *conn, STRING_OPEN_FILE, name);
					print_line_to_log_file(log, log_msg);
				}
				else
				{
					int res = create_file_struct(&files[nof], name, owner);
					if (res == -1)
					{
						//file error!
						SEND_TO_SOCKET(*conn, &out_msg);
						destroy_message(&in_msg);

						//logging
						snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
							"File error.", *conn, STRING_OPEN_FILE, name);
						print_line_to_log_file(log, log_msg);
						return -1;
					}

					out_msg.type |= MESSAGE_OP_SUCC;

					if (flags & MESSAGE_OPEN_OLOCK)
						strncpy(lastop_writefile_pname, name, name_size);

					//logging
					snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
						"Success.", *conn, STRING_OPEN_FILE, name);
					print_line_to_log_file(log, log_msg);
				}
			}
			else
			{
				//file already exists;
				out_msg.type |= MESSAGE_FILE_EXISTS;

				//logging
				snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
					"File exists.", *conn, STRING_OPEN_FILE, name);
				print_line_to_log_file(log, log_msg);
			}
		}
		else	//open existing file
		{
			if (fex >= 0)
			{
				//file exists
				int opn = open_file(&files[fex], owner);
				if (opn == 0)
				{
					out_msg.type |= MESSAGE_OP_SUCC;

					//logging
					snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
						"Success.", *conn, STRING_OPEN_FILE, name);
					print_line_to_log_file(log, log_msg);
				}
				else if(errno == EPERM)
				{
					out_msg.type |= MESSAGE_FILE_NPERM;

					//logging
					snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
						"Permission denied.", *conn, STRING_OPEN_FILE, name);
					print_line_to_log_file(log, log_msg);
				}
				else
				{
					//file error!
					SEND_TO_SOCKET(*conn, &out_msg);
					destroy_message(&in_msg);

					//logging
					snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
						"File error.", *conn, STRING_OPEN_FILE, name);
					print_line_to_log_file(log, log_msg);
					return -1;
				}
			}
			else
			{
				//file doesn't exist
				out_msg.type |= MESSAGE_FILE_NEXISTS;

				//logging
				snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
					"File doesn't exist.", *conn, STRING_OPEN_FILE, name);
				print_line_to_log_file(log, log_msg);
			}
		}
	}

	//CLOSE FILE --------------------------------------------------------------
	else if(in_msg.type & MESSAGE_CLOSE_FILE)
	{
		size_t name_size; char* name;
		ERRCHECK(read_file_name(&in_msg, &name, &name_size));

		out_msg.type = MESSAGE_CFILE_ACK;

		int fex = file_exists(files, file_num, name);
		if (fex >= 0)
		{
			int lck = 0; //close_file(&files[fex], *conn);
			if (lck == 0)
			{
				out_msg.type |= MESSAGE_OP_SUCC;

				//logging
				snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
					"Success.", *conn, STRING_CLOSE_FILE, name);
				print_line_to_log_file(log, log_msg);
			}
			else if (errno == EPERM)
			{
				out_msg.type |= MESSAGE_FILE_LOCK;

				//logging
				snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
					"Permission denied.", *conn, STRING_CLOSE_FILE, name);
				print_line_to_log_file(log, log_msg);
			}
			else
			{
				//file error!
				SEND_TO_SOCKET(*conn, &out_msg);
				destroy_message(&in_msg);

				//logging
				snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
					"File error.", *conn, STRING_CLOSE_FILE, name);
				print_line_to_log_file(log, log_msg);
				return -1;
			}
		}
		else if (errno == ENOENT)
		{
			out_msg.type |= MESSAGE_FILE_NEXISTS;

			//logging
			snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
				"File doesnt not exist.", *conn, STRING_CLOSE_FILE, name);
			print_line_to_log_file(log, log_msg);
		}
		else
		{
			//file error!
			SEND_TO_SOCKET(*conn, &out_msg);
			destroy_message(&in_msg);

			//logging
			snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
				"File error.", *conn, STRING_CLOSE_FILE, name);
			print_line_to_log_file(log, log_msg);
			return -1;
		}
	}
	else if(in_msg.type & MESSAGE_READ_FILE)
	{

	}
	else if (in_msg.type & MESSAGE_READN_FILE)
	{

	}
	else if(in_msg.type & MESSAGE_WRITE_FILE)
	{

	}
	else if(in_msg.type & MESSAGE_APPEND_FILE)
	{

	}

	//LOCK FILE ---------------------------------------------------------------
	else if(in_msg.type & MESSAGE_LOCK_FILE)
	{
		size_t name_size; char* name;
		ERRCHECK(read_file_name(&in_msg, &name, &name_size));

		out_msg.type = MESSAGE_LFILE_ACK;

		int fex = file_exists(files, file_num, name);
		if (fex >= 0)
		{
			int lck = lock_file(&files[fex], *conn);
			if (lck == 0)
			{
				out_msg.type |= MESSAGE_OP_SUCC;

				//logging
				snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
					"Success.", *conn, STRING_LOCK_FILE, name);
				print_line_to_log_file(log, log_msg);
			}
			else if (errno == EPERM)
			{
				out_msg.type |= MESSAGE_FILE_NOWN;

				//logging
				snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
					"Permission denied.", *conn, STRING_LOCK_FILE, name);
				print_line_to_log_file(log, log_msg);
			}
		}
		else if (errno == ENOENT)
		{
			out_msg.type |= MESSAGE_FILE_NEXISTS;

			//logging
			snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
				"File doesn't exist.", *conn, STRING_LOCK_FILE, name);
			print_line_to_log_file(log, log_msg);
		}
		else
		{
			//file error!
			SEND_TO_SOCKET(*conn, &out_msg);
			destroy_message(&in_msg);

			//logging
			snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
				"File error.", *conn, STRING_LOCK_FILE, name);
			print_line_to_log_file(log, log_msg);
			return -1;
		}
	}

	//UNLOCK FILE -------------------------------------------------------------
	else if(in_msg.type & MESSAGE_UNLOCK_FILE)
	{
		size_t name_size; char* name;
		read_file_name(&in_msg, &name, &name_size);

		out_msg.type = MESSAGE_ULFILE_ACK;
		int fex = file_exists(files, file_num, name);
		if (fex >= 0)
		{
			int lck = unlock_file(&files[fex], *conn);
			if (lck == 0)
			{
				out_msg.type |= MESSAGE_OP_SUCC;

				//logging
				snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
					"Success.", *conn, STRING_UNLOCK_FILE, name);
				print_line_to_log_file(log, log_msg);
			}
			else if (errno == EPERM)
			{
				out_msg.type |= MESSAGE_FILE_NOWN;

				//logging
				snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
					"Permission denied.", *conn, STRING_UNLOCK_FILE, name);
				print_line_to_log_file(log, log_msg);
			}
		}
		else if (errno == ENOENT)
		{
			out_msg.type |= MESSAGE_FILE_NEXISTS;

			//logging
			snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
				"File doesn't exist.", *conn, STRING_UNLOCK_FILE, name);
			print_line_to_log_file(log, log_msg);
		}
		else
		{
			//file error!
			SEND_TO_SOCKET(*conn, &out_msg);
			destroy_message(&in_msg);

			//logging
			snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
				"File error.", *conn, STRING_UNLOCK_FILE, name);
			print_line_to_log_file(log, log_msg);
			return -1;
		}
	}

	//REMOVE FILE -------------------------------------------------------------
	else if(in_msg.type & MESSAGE_REMOVE_FILE)
	{
		size_t name_size; char* name;
		read_file_name(&in_msg, &name, &name_size);

		out_msg.type = MESSAGE_RMFILE_ACK;
		int fex = file_exists(files, file_num, name);
		if (fex >= 0)
		{
			if(files[fex].owner == OWNER_NULL)
				out_msg.type |= MESSAGE_FILE_NLOCK;
			else
			{
				int lck = remove_file(&files[fex], *conn);
				if (lck == 0)
				{
					out_msg.type |= MESSAGE_OP_SUCC;

					//logging
					snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
						"Success.", *conn, STRING_REMOVE_FILE, name);
					print_line_to_log_file(log, log_msg);
				}
				else if (errno == EPERM)
				{
					out_msg.type |= MESSAGE_FILE_NOWN;

					//logging
					snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
						"Permission denied.", *conn, STRING_REMOVE_FILE, name);
					print_line_to_log_file(log, log_msg);
				}
			}
		}
		else if (errno == ENOENT)
		{
			out_msg.type |= MESSAGE_FILE_NEXISTS;

			//logging
			snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
				"File doesn't exist.", *conn, STRING_REMOVE_FILE, name);
			print_line_to_log_file(log, log_msg);
		}
		else
		{
			//file error!
			SEND_TO_SOCKET(*conn, &out_msg);
			destroy_message(&in_msg);

			//logging
			snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Op: %s; File: %s;"
				"File error.", *conn, STRING_REMOVE_FILE, name);
			print_line_to_log_file(log, log_msg);
			return -1;
		}
	}

	//UNKNOWN MESSAGE ---------------------------------------------------------
	else
	{
		out_msg.type = MESSAGE_NULL;

		snprintf(log_msg, FILENAME_MAX_SIZE, "Client: %d; Unkown Operation.", *conn);
		print_line_to_log_file(log, log_msg);
	}

	SEND_TO_SOCKET(*conn, &out_msg);
	destroy_message(&in_msg);
	return 0;
}

//returns the index of the matching file.
static int file_exists(file_t* files, size_t file_num, const char* filename)
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

//returns the index of the first non existing file.
static int get_no_file(file_t* files, size_t file_num)
{
	for (size_t i = 0; i < file_num; i++)
	{
		int val = is_existing_file(&files[i]);
		if (val == 1) return i;
		else if (val != 0) return -1;
	}
	ERRSET(EMFILE, -1);
}

static int read_file_name(net_msg* msg, char** file_name, size_t* file_size)
{
	pop_buf(&msg->data, sizeof(size_t), file_size);
	if (*file_size > FILENAME_MAX) *file_size = FILENAME_MAX;
	MALLOC(*file_name, *file_size);
	pop_buf(&msg->data, *file_size * sizeof(char), *file_name);
	return 0;
}

static int evict_FIFO(file_t* files, size_t file_num, size_t* last_evicted,
	long curr_st, int curr_f, pthread_mutex_t* state_mux)
{
	size_t curr_older = 0;
	time_t curr_older_time = time(NULL);
	time_t temp;
	for (size_t i = 0; i < file_num; i++)
	{
		get_usage_data(&files[i], &temp, NULL, NULL);
		if (temp < curr_older_time)
		{
			curr_older_time = temp;
			curr_older = i;
		}
	}

	size_t evict_size;
	get_size(&files[curr_older], &evict_size);

	ERRCHECK(pthread_mutex_lock(state_mux));
	curr_st -= evict_size;
	if (curr_f > 0) curr_f--;
	force_remove_file(&files[curr_older]);
	*last_evicted = curr_older;
	ERRCHECK(pthread_mutex_unlock(state_mux));

	return 0;
}

static int evict_LRU(file_t* files, size_t file_num, size_t* last_evicted,
	long curr_st, int curr_f, pthread_mutex_t* state_mux)
{
	ERRCHECK(pthread_mutex_lock(state_mux));
	size_t clock_pos = *last_evicted;
	ERRCHECK(pthread_mutex_unlock(state_mux));
	char temp = 1;

	for (size_t i = 0; i < file_num && temp; i++)
	{
		get_usage_data(&files[clock_pos], NULL, NULL, &temp);
		if (temp)
		{
			update_lru(&files[clock_pos], 0);
			clock_pos = (clock_pos + 1) % file_num;
		}
	}

	size_t evict_size;
	get_size(&files[clock_pos], &evict_size);

	ERRCHECK(pthread_mutex_lock(state_mux));
	curr_st -= evict_size;
	if (curr_f > 0) curr_f--;
	force_remove_file(&files[clock_pos]);
	*last_evicted = clock_pos;
	ERRCHECK(pthread_mutex_unlock(state_mux));

	return 0;
}

static int evict_LFU(file_t* files, size_t file_num, size_t* last_evicted,
	long curr_st, int curr_f, pthread_mutex_t* state_mux)
{
	size_t curr_least_used = 0;
	int curr_least_used_freq = INT_MAX;
	int temp;
	for (size_t i = 0; i < file_num; i++)
	{
		get_usage_data(&files[i], NULL, &temp, NULL);
		if (temp < curr_least_used_freq)
		{
			curr_least_used_freq = temp;
			curr_least_used = i;
		}
	}

	size_t evict_size;
	get_size(&files[curr_least_used], &evict_size);

	ERRCHECK(pthread_mutex_lock(state_mux));
	curr_st -= evict_size;
	if (curr_f > 0) curr_f--;
	force_remove_file(&files[curr_least_used]);
	*last_evicted = curr_least_used;
	ERRCHECK(pthread_mutex_unlock(state_mux));

	return 0;
}