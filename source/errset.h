#ifndef GENERIC_ERRSET
#define GENERIC_ERRSET

//this is a macro to set an errno and return a function with a given (int) value, typically -1.
#define ERRSET(err, retcode) { errno = err; return retcode; }

#define ERRCHECK(cond) if(cond == -1) return -1;
#define ERRCKDO(cond, func) if(cond == -1) { func; return -1; }
#define ERRCHECKDO(cond, func) ERRCKDO(cond, func)

#endif
