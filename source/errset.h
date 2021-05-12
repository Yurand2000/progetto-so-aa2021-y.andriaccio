#ifndef GENERIC_ERRSET
#define GENERIC_ERRSET

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

//this is a macro to set an errno and return a function with a given (int) value, typically 0.
#define ERRSET(err, retcode) { errno = err; return retcode; }
#define ERRSETDO(err, func, retcode) { errno = err; func; return retcode; }

#define PTRCHECK(ptr) { if(ptr == NULL) return -1; }
#define PTRCHECKERRSET(ptr, err, retcode) { if(ptr == NULL) ERRSET(err, retcode); }
#define PTRCHECKDO(ptr, func) { if(ptr == NULL) { func; return -1;  } }
#define PTRCHECKERRSETDO(ptr, err, func, retcode) { if(ptr == NULL)\
       	ERRSETDO(err, func, retcode); }

#define MALLOC(ptr, size) { ptr = malloc(size); PTRCHECK(ptr); }
#define MALLOCDO(ptr, size, func) { ptr = malloc(size); PTRCHECKDO(ptr, func); }
#define REALLOC(ptrl, ptrr, size) { ptrl = realloc(ptrr, size); PTRCHECK(ptrl); }
#define REALLOCDO(ptrl, ptrr, size, func) { ptrl = realloc(ptrr, size);\
	PTRCHECKDO(ptrl, func); };

#define ERRCHECK(cond) if(cond == -1) return -1;
#define ERRCKDO(cond, func) if(cond == -1) { func; return -1; }
#define ERRCHECKDO(cond, func) ERRCKDO(cond, func)

#define PERRCHECK(cond, err) if(cond) { perror(err); _exit(1); }

#endif
