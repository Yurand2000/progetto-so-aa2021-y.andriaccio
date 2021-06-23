#ifndef READN_WRITEN
#define READN_WRITEN
#include "win32defs.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

/* “Advanced Programming In the UNIX Environment”
 * by W. Richard Stevens and Stephen A. Rago, 2013, 3rd Edition, Addison-Wesley
 *
 * Pipes, FIFOs, and some devices—notably terminals and networks—have the
 * following two properties.
 * 1. A read operation may return less than asked for, even though we have not
 *    encountered the end of file. This is not an error, and we should simply
 *    continue reading from the device.
 * 2. A write operation can return less than we specified. This may be caused
 *    by kernel output buffers becoming full, for example. Again, it’s not an
 *    error, and we should continue writing the remainder of the data.
 *    (Normally, this short return from a write occurs only with a nonblocking
 *    descriptor or if a signal is caught.)
 * We’ll never see this happen when reading or writing a disk file, except when
 * the file system runs out of space or we hit our quota limit and we can’t
 * write all that we requested. Generally, when we read from or write to a
 * pipe, network device, or terminal, we need to take these characteristics
 * into consideration. We can use the readn and writen functions to read and
 * write N bytes of data, respectively, letting these functions handle a return
 * value that’s possibly less than requested. These two functions simply call
 * read or write as many times as required to read or write the entire N bytes
 * of data. We call writen whenever we’re writing to one of the file types that
 * we mentioned, but we call readn only when we know ahead of time that we will
 * be receiving a certain number of bytes. Figure 14.24 shows implementations
 * of readn and writen that we will use in later examples. Note that if we
 * encounter an error and have previously read or written any data, we return
 * the amount of data transferred instead of the error. Similarly, if we reach
 * the end of file while reading, we return the number of bytes copied to the
 * caller’s buffer if we already read some data successfully and have not yet
 * satisfied the amount requested. */

ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, void *ptr, size_t n);

#endif