#ifndef WIN32DEFS
#define WIN32DEFS

#ifdef WIN32

typedef int ssize_t;
typedef int pthread_mutex_t;

int pthread_mutex_init(pthread_mutex_t* mux, void*);
int pthread_mutex_destroy(pthread_mutex_t* mux);
int pthread_mutex_lock(pthread_mutex_t* mux);
int pthread_mutex_unlock(pthread_mutex_t* mux);

#endif

#endif