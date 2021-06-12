#ifndef NETWORK_CLIENT_API
#define NETWORK_CLIENT_API

#include <time.h>

#define O_CREATE 1
#define O_LOCK   2

/* This is the required API that allows the client to communicate with the server.
 * No other functions are used by softwares to connect to the server.
 * they return 0 on success, -1 on error setting errno. */

/* Open AF_UNIX connection to the given socket, with a retry time of msec, and a
 * timeout of abstime. */
int openConnection(const char* sockname, int msec, const struct timespec abstime);

/* Closes the connetion if open, the parameter sockname is ignored. */
int closeConnection(const char* sockname);

/* Request to open or create a file, given the file pathname and optional flags.
 * FLAGS: O_CREATE, O_LOCK. */
int openFile(const char* pathname, int flags);

/* Request to read the whole content of the given file, storing it into the
 * given buffer. (Automatic reallocation of buf) */
int readFile(const char* pathname, void** buf, size_t* size);

/* Request to read N random files from the file storage, storing it into the
 * given directory. if N <= 0, the server will send all files. 
 * returns 0 or more files actually read, -1 on error and sets errno. */
int readNFiles(int n, const char* dirname);

/* Request to write a file into the server files. If the file expels some data,
 * it is returned to the caller. Valid only if the prevoius call was openFile
 * with O_CREATE as flag, same pathname, and returned with success. */
int writeFile(const char* pathname, const char* dirname);

/* Request to append data to a given file. If the file expels some data, it is
 * returned to the caller. */
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

/* Request to lock a file for exclusive use. */
int lockFile(const char* pathname);

/* Request to unlock a file that had exclusive use. */
int unlockFile(const char* pathname);

/* Request to close an open file for the user. */
int closeFile(const char* pathname);

/* Request to delete a file from the server. */
int removeFile(const char* pathname);

#endif
