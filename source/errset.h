#ifndef GENERIC_ERRSET
#define GENERIC_ERRSET

//this is a macro to set an errno and return a function with a given (int) value, typically -1.
#define ERRSET(err, retcode) { errno = err; return retcode; }
#define ERRSETDO(err, func, retcode) { errno = err; func; return retcode; }

#define MALLOC(ptr, size) { ptr = malloc(size); if(ptr == NULL) return -1; }
#define MALLOCDO(ptr, size, func) { ptr = malloc(size); if(ptr == NULL) { func; return -1; } }

#define ERRCHECK(cond) if(cond == -1) return -1;
#define ERRCKDO(cond, func) if(cond == -1) { func; return -1; }
#define ERRCHECKDO(cond, func) ERRCKDO(cond, func)

#endif
