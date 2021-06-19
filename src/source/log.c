#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "errset.h"

#define LOCK(mux) ERRCHECK(pthread_mutex_lock(mux))
#define UNLOCK(mux) ERRCHECK(pthread_mutex_unlock(mux))

int init_log_file_struct(log_t* log, char* logname)
{
	PTRCHECKERRSET(log, EINVAL, -1);
	PTRCHECKERRSET(logname, EINVAL, -1);

	size_t len = strlen(logname);
	if (len >= FILENAME_MAX_SIZE) ERRSET(EINVAL, -1);

	ERRCHECK(pthread_mutex_init(&log->mux, NULL));
	strncpy(log->filename, logname, len + 1);
	return 0;
}

int destroy_log_file_struct(log_t* log)
{
	PTRCHECKERRSET(log, EINVAL, -1);

	ERRCHECK(pthread_mutex_destroy(&log->mux));
	log->filename[0] = '\0';
	return 0;
}

int reset_log(log_t* log)
{
	PTRCHECKERRSET(log, EINVAL, -1);
	LOCK(&log->mux);

	FILE* logfile = fopen(log->filename, "w");
	PTRCHECK(logfile);

	time_t currtime; struct tm time_print;
	time(&currtime);
	localtime_r(&currtime, &time_print);

	fprintf(logfile, "[%4d/%2d/%2d-%2d:%2d:%2d] Opening log file.\n",
		(time_print.tm_year + 1900), (time_print.tm_mon + 1), time_print.tm_mday,
		time_print.tm_hour, time_print.tm_min, time_print.tm_sec);
	fclose(logfile);

	UNLOCK(&log->mux);
	return 0;
}

int print_line_to_log_file(log_t* log, char* line)
{
	PTRCHECKERRSET(log, EINVAL, -1);
	PTRCHECKERRSET(line, EINVAL, -1);
	LOCK(&log->mux);

	FILE* logfile = fopen(log->filename, "a");
	PTRCHECK(logfile);

	time_t currtime; struct tm time_print;
	time(&currtime);
	localtime_r(&currtime, &time_print);

	fprintf(logfile, "[%4d/%2d/%2d-%2d:%2d:%2d] %s\n",
		(time_print.tm_year + 1900), (time_print.tm_mon + 1), time_print.tm_mday,
		time_print.tm_hour, time_print.tm_min, time_print.tm_sec, line);
	fclose(logfile);

	UNLOCK(&log->mux);
	return 0;
}