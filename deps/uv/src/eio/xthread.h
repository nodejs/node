#ifndef XTHREAD_H_
#define XTHREAD_H_

/* whether word reads are potentially non-atomic.
 * this is conservative, likely most arches this runs
 * on have atomic word read/writes.
 */
#ifndef WORDACCESS_UNSAFE
# if __i386 || __x86_64
#  define WORDACCESS_UNSAFE 0
# else
#  define WORDACCESS_UNSAFE 1
# endif
#endif

/////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

#include <stdio.h>//D
#include <fcntl.h>
#include <io.h>
#include <time.h>
#include <winsock2.h>
#include <process.h>
#include <windows.h>
#include <pthread.h>
#define sigset_t int
#define sigfillset(a)
#define pthread_sigmask(a,b,c)
#define sigaddset(a,b)
#define sigemptyset(s)

typedef pthread_mutex_t xmutex_t;
#define X_MUTEX_INIT           PTHREAD_MUTEX_INITIALIZER
#define X_MUTEX_CREATE(mutex)  pthread_mutex_init (&(mutex), 0)
#define X_LOCK(mutex)          pthread_mutex_lock (&(mutex))
#define X_UNLOCK(mutex)        pthread_mutex_unlock (&(mutex))

typedef pthread_cond_t xcond_t;
#define X_COND_INIT                     PTHREAD_COND_INITIALIZER
#define X_COND_CREATE(cond)		pthread_cond_init (&(cond), 0)
#define X_COND_SIGNAL(cond)             pthread_cond_signal (&(cond))
#define X_COND_WAIT(cond,mutex)         pthread_cond_wait (&(cond), &(mutex))
#define X_COND_TIMEDWAIT(cond,mutex,to) pthread_cond_timedwait (&(cond), &(mutex), &(to))

typedef pthread_t xthread_t;
#define X_THREAD_PROC(name) void *name (void *thr_arg)
#define X_THREAD_ATFORK(a,b,c)

static int
thread_create (xthread_t *tid, void *(*proc)(void *), void *arg)
{
  int retval;
  pthread_attr_t attr;

  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);

  retval = pthread_create (tid, &attr, proc, arg) == 0;

  pthread_attr_destroy (&attr);

  return retval;
}

#define respipe_read(a,b,c)  PerlSock_recv ((a), (b), (c), 0)
#define respipe_write(a,b,c) send ((a), (b), (c), 0)
#define respipe_close(a)     PerlSock_closesocket ((a))

#else
/////////////////////////////////////////////////////////////////////////////

#if __linux && !defined(_GNU_SOURCE)
# define _GNU_SOURCE
#endif

/* just in case */
#define _REENTRANT 1

#if __solaris
# define _POSIX_PTHREAD_SEMANTICS 1
/* try to bribe solaris headers into providing a current pthread API
 * despite environment being configured for an older version.
 */
# define __EXTENSIONS__ 1
#endif

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <pthread.h>

typedef pthread_mutex_t xmutex_t;
#if __linux && defined (PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP)
# define X_MUTEX_INIT		PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
# define X_MUTEX_CREATE(mutex)						\
  do {									\
    pthread_mutexattr_t attr;						\
    pthread_mutexattr_init (&attr);					\
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_ADAPTIVE_NP);	\
    pthread_mutex_init (&(mutex), &attr);				\
  } while (0)
#else
# define X_MUTEX_INIT		PTHREAD_MUTEX_INITIALIZER
# define X_MUTEX_CREATE(mutex)	pthread_mutex_init (&(mutex), 0)
#endif
#define X_LOCK(mutex)		pthread_mutex_lock   (&(mutex))
#define X_UNLOCK(mutex)		pthread_mutex_unlock (&(mutex))

typedef pthread_cond_t xcond_t;
#define X_COND_INIT			PTHREAD_COND_INITIALIZER
#define X_COND_CREATE(cond)		pthread_cond_init (&(cond), 0)
#define X_COND_SIGNAL(cond)		pthread_cond_signal (&(cond))
#define X_COND_WAIT(cond,mutex)		pthread_cond_wait (&(cond), &(mutex))
#define X_COND_TIMEDWAIT(cond,mutex,to)	pthread_cond_timedwait (&(cond), &(mutex), &(to))

typedef pthread_t xthread_t;
#define X_THREAD_PROC(name) static void *name (void *thr_arg)
#define X_THREAD_ATFORK(prepare,parent,child) pthread_atfork (prepare, parent, child)

// the broken bsd's once more
#ifndef PTHREAD_STACK_MIN
# define PTHREAD_STACK_MIN 0
#endif

#ifndef XTHREAD_STACKSIZE
# define XTHREAD_STACKSIZE sizeof (void *) * 4096
#endif

static int
thread_create (xthread_t *tid, void *(*proc)(void *), void *arg)
{
  int retval;
  sigset_t fullsigset, oldsigset;
  pthread_attr_t attr;

  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
  pthread_attr_setstacksize (&attr, PTHREAD_STACK_MIN < X_STACKSIZE ? X_STACKSIZE : PTHREAD_STACK_MIN);
#ifdef PTHREAD_SCOPE_PROCESS
  pthread_attr_setscope (&attr, PTHREAD_SCOPE_PROCESS);
#endif

  sigfillset (&fullsigset);

  pthread_sigmask (SIG_SETMASK, &fullsigset, &oldsigset);
  retval = pthread_create (tid, &attr, proc, arg) == 0;
  pthread_sigmask (SIG_SETMASK, &oldsigset, 0);

  pthread_attr_destroy (&attr);

  return retval;
}

#define respipe_read(a,b,c)  read  ((a), (b), (c))
#define respipe_write(a,b,c) write ((a), (b), (c))
#define respipe_close(a)     close ((a))

#endif

#endif

