/*
 * libeio implementation
 *
 * Copyright (c) 2007,2008,2009,2010,2011 Marc Alexander Lehmann <libeio@schmorp.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 * 
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License ("GPL") version 2 or any later version,
 * in which case the provisions of the GPL are applicable instead of
 * the above. If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the BSD license, indicate your decision
 * by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL. If you do not delete the
 * provisions above, a recipient may use your version of this file under
 * either the BSD or the GPL.
 */

#ifdef EIO_CONFIG_H
# include EIO_CONFIG_H
#endif

/*  Undone by libuv for easy build scripts.
 * #ifndef _WIN32
 * # include "config.h"
 * #endif
 */

#include "eio.h"
#include "ecb.h"

#ifdef EIO_STACKSIZE
# define X_STACKSIZE EIO_STACKSIZE
#endif
#include "xthread.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <assert.h>

/* intptr_t comes from unistd.h, says POSIX/UNIX/tradition */
/* intptr_t only comes from stdint.h, says idiot openbsd coder */
#if HAVE_STDINT_H
# include <stdint.h>
#endif

#ifndef ECANCELED
# define ECANCELED EDOM
#endif
#ifndef ELOOP
# define ELOOP EDOM
#endif

#if !defined(ENOTSOCK) && defined(WSAENOTSOCK)
# define ENOTSOCK WSAENOTSOCK
#endif

static void eio_destroy (eio_req *req);

#ifndef EIO_FINISH
# define EIO_FINISH(req)  ((req)->finish) && !EIO_CANCELLED (req) ? (req)->finish (req) : 0
#endif

#ifndef EIO_DESTROY
# define EIO_DESTROY(req) do { if ((req)->destroy) (req)->destroy (req); } while (0)
#endif

#ifndef EIO_FEED
# define EIO_FEED(req)    do { if ((req)->feed   ) (req)->feed    (req); } while (0)
#endif

#ifndef EIO_FD_TO_WIN32_HANDLE
# define EIO_FD_TO_WIN32_HANDLE(fd) _get_osfhandle (fd)
#endif
#ifndef EIO_WIN32_HANDLE_TO_FD
# define EIO_WIN32_HANDLE_TO_FD(handle) _open_osfhandle (handle, 0)
#endif

#define EIO_ERRNO(errval,retval) ((errno = errval), retval)

#define EIO_ENOSYS() EIO_ERRNO (ENOSYS, -1)

#ifdef __sun
# define futimes(fd, times) futimesat (fd, NULL, times)
#endif

#ifdef _WIN32

  #include <direct.h>

  #undef PAGESIZE
  #define PAGESIZE 4096 /* GetSystemInfo? */

  /* TODO: look at how perl does stat (non-sloppy), unlink (ro-files), utime, link */

  #ifdef EIO_STRUCT_STATI64
    /* look at perl's non-sloppy stat */
    #define stat(path,buf)       _stati64 (path,buf)
    #define fstat(fd,buf)        _fstati64 (fd,buf)
  #endif
  #define lstat(path,buf)      stat (path,buf)
  #define fsync(fd)            (FlushFileBuffers ((HANDLE)EIO_FD_TO_WIN32_HANDLE (fd)) ? 0 : EIO_ERRNO (EBADF, -1))
  #define mkdir(path,mode)     _mkdir (path)
  #define link(old,neu)        (CreateHardLink (neu, old, 0) ? 0 : EIO_ERRNO (ENOENT, -1))

  #define chmod(path,mode)     _chmod (path, mode)
  #define dup(fd)              _dup (fd)
  #define dup2(fd1,fd2)        _dup2 (fd1, fd2)

  #define fchmod(fd,mode)      EIO_ENOSYS ()
  #define chown(path,uid,gid)  EIO_ENOSYS ()
  #define fchown(fd,uid,gid)   EIO_ENOSYS ()
  #define truncate(path,offs)  EIO_ENOSYS () /* far-miss: SetEndOfFile */
  #define ftruncate(fd,offs)   EIO_ENOSYS () /* near-miss: SetEndOfFile */
  #define mknod(path,mode,dev) EIO_ENOSYS ()
  #define sync()               EIO_ENOSYS ()
  #define readlink(path,buf,s) EIO_ENOSYS ()
  #define statvfs(path,buf)    EIO_ENOSYS ()
  #define fstatvfs(fd,buf)     EIO_ENOSYS ()

  #define getcwd(buf,s)        _getcwd(buf, s)
  #define rmdir(path)          _rmdir(path)

  /* rename() uses MoveFile, which fails to overwrite */
  #define rename(old,neu)      eio__rename (old, neu)

  static int
  eio__rename (const char *old, const char *neu)
  {
    if (MoveFileEx (old, neu, MOVEFILE_REPLACE_EXISTING))
      return 0;

    /* should steal _dosmaperr */
    switch (GetLastError ())
      {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_DRIVE:
        case ERROR_NO_MORE_FILES:
        case ERROR_BAD_NETPATH:
        case ERROR_BAD_NET_NAME:
        case ERROR_BAD_PATHNAME:
        case ERROR_FILENAME_EXCED_RANGE:
          errno = ENOENT;
          break;

        default:
          errno = EACCES;
          break;
      }

    return -1;
  }

  /* we could even stat and see if it exists */
  static int
  symlink (const char *old, const char *neu)
  {
    #if WINVER >= 0x0600
      if (CreateSymbolicLink (neu, old, 1))
        return 0;

      if (CreateSymbolicLink (neu, old, 0))
        return 0;
    #endif

    return EIO_ERRNO (ENOENT, -1);
  }

  /* POSIX API only */
  #ifndef CreateHardLink
  # define CreateHardLink(neu,old,flags) 0
  #endif
  #define CreateSymbolicLink(neu,old,flags) 0

  struct statvfs
  {
    int dummy;
  };

  #define DT_DIR EIO_DT_DIR
  #define DT_REG EIO_DT_REG
  #define D_NAME(entp) entp.cFileName
  #define D_TYPE(entp) (entp.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? DT_DIR : DT_REG)

#include <sys/utime.h>
#define utime(path, times)   _utime(path, times)
#define utimbuf              _utimbuf

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

struct timezone 
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};

static int gettimeofday(struct timeval *tv, struct timezone *tz)
{
  FILETIME ft;
  unsigned __int64 tmpres = 0;
  static int tzflag;

  if (NULL != tv)
  {
    GetSystemTimeAsFileTime(&ft);

    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;

    /*converting file time to unix epoch*/
    tmpres -= DELTA_EPOCH_IN_MICROSECS; 
    tmpres /= 10;  /*convert into microseconds*/
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }

  if (NULL != tz)
  {
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
    tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
  }

  return 0;
}

#else

  #include <sys/time.h>
  #include <sys/select.h>
  #include <sys/statvfs.h>
  #include <unistd.h>
  #include <signal.h>
  #include <dirent.h>

  #if _POSIX_MEMLOCK || _POSIX_MEMLOCK_RANGE || _POSIX_MAPPED_FILES
    #include <sys/mman.h>
  #endif

  #define D_NAME(entp) entp->d_name

  /* POSIX_SOURCE is useless on bsd's, and XOPEN_SOURCE is unreliable there, too */
  #if __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__
    #define _DIRENT_HAVE_D_TYPE /* sigh */
    #define D_INO(de) (de)->d_fileno
    #define D_NAMLEN(de) (de)->d_namlen
  #elif __linux || defined d_ino || _XOPEN_SOURCE >= 600
    #define D_INO(de) (de)->d_ino
  #endif

  #ifdef _D_EXACT_NAMLEN
    #undef D_NAMLEN
    #define D_NAMLEN(de) _D_EXACT_NAMLEN (de)
  #endif

  #ifdef _DIRENT_HAVE_D_TYPE
    #define D_TYPE(de) (de)->d_type
  #endif

  #ifndef EIO_STRUCT_DIRENT
    #define EIO_STRUCT_DIRENT struct dirent
  #endif

#endif

#if HAVE_UTIMES
# include <utime.h>
#endif

#if HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif

#if HAVE_SYS_PRCTL_H
# include <sys/prctl.h>
#endif

#if HAVE_SENDFILE
# if __linux
#  include <sys/sendfile.h>
# elif __FreeBSD__ || __DragonFly__ || defined __APPLE__
#  include <sys/socket.h>
#  include <sys/uio.h>
# elif __hpux
#  include <sys/socket.h>
# elif __solaris
#  include <sys/sendfile.h>
# else
#  error sendfile support requested but not available
# endif
#endif

#ifndef D_TYPE
# define D_TYPE(de) 0
#endif
#ifndef D_INO
# define D_INO(de) 0
#endif
#ifndef D_NAMLEN
# define D_NAMLEN(entp) strlen (D_NAME (entp))
#endif

/* used for struct dirent, AIX doesn't provide it */
#ifndef NAME_MAX
# define NAME_MAX 4096
#endif

/* used for readlink etc. */
#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

/* buffer size for various temporary buffers */
#define EIO_BUFSIZE 65536

#define dBUF	 				\
  char *eio_buf = malloc (EIO_BUFSIZE);		\
  errno = ENOMEM;				\
  if (!eio_buf)					\
    return -1

#define FUBd					\
  free (eio_buf)

#define EIO_TICKS ((1000000 + 1023) >> 10)

#define ETP_PRI_MIN EIO_PRI_MIN
#define ETP_PRI_MAX EIO_PRI_MAX

struct etp_worker;

#define ETP_REQ eio_req
#define ETP_DESTROY(req) eio_destroy (req)
static int eio_finish (eio_req *req);
#define ETP_FINISH(req)  eio_finish (req)
static void eio_execute (struct etp_worker *self, eio_req *req);
#define ETP_EXECUTE(wrk,req) eio_execute (wrk,req)

/*****************************************************************************/

#define ETP_NUM_PRI (ETP_PRI_MAX - ETP_PRI_MIN + 1)

/* calculate time difference in ~1/EIO_TICKS of a second */
ecb_inline int
tvdiff (struct timeval *tv1, struct timeval *tv2)
{
  return  (tv2->tv_sec  - tv1->tv_sec ) * EIO_TICKS
       + ((tv2->tv_usec - tv1->tv_usec) >> 10);
}

static unsigned int started, idle, wanted = 4;

static void (*want_poll_cb) (void);
static void (*done_poll_cb) (void);
 
static unsigned int max_poll_time;     /* reslock */
static unsigned int max_poll_reqs;     /* reslock */

static unsigned int nreqs;    /* reqlock */
static unsigned int nready;   /* reqlock */
static unsigned int npending; /* reqlock */
static unsigned int max_idle = 4;      /* maximum number of threads that can idle indefinitely */
static unsigned int idle_timeout = 10; /* number of seconds after which an idle threads exit */

static xmutex_t wrklock;
static xmutex_t reslock;
static xmutex_t reqlock;
static xcond_t  reqwait;

/* Fix for test-fs-sir-writes-alot */
/* Apple's OSX can't safely write() concurrently from 2 threads */
/* for more info see the thread "fs.write Data Munging" in the nodejs google group */
/* http://groups.google.com/group/nodejs/browse_thread/thread/c11f8b683f37cef/b18ad9e0a15314c5 */
/* And the thread "write()s and pwrite()s from multiple threads in OSX" in libev@lists.schmorp.de */
/* http://lists.schmorp.de/pipermail/libev/2010q4/001185.html */
#if defined (__APPLE__)
static xmutex_t apple_bug_writelock = X_MUTEX_INIT;
#endif

#if !HAVE_PREADWRITE
/*
 * make our pread/pwrite emulation safe against themselves, but not against
 * normal read/write by using a mutex. slows down execution a lot,
 * but that's your problem, not mine.
 */
static xmutex_t preadwritelock;
#endif

typedef struct etp_worker
{
  /* locked by wrklock */
  struct etp_worker *prev, *next;

  xthread_t tid;

  /* locked by reslock, reqlock or wrklock */
  ETP_REQ *req; /* currently processed request */

#ifdef ETP_WORKER_COMMON
  ETP_WORKER_COMMON
#endif
} etp_worker;

static etp_worker wrk_first; /* NOT etp */

#define ETP_WORKER_LOCK(wrk)   X_LOCK   (wrklock)
#define ETP_WORKER_UNLOCK(wrk) X_UNLOCK (wrklock)

/* worker threads management */

static void ecb_cold
etp_worker_clear (etp_worker *wrk)
{
}

static void ecb_cold
etp_worker_free (etp_worker *wrk)
{
  wrk->next->prev = wrk->prev;
  wrk->prev->next = wrk->next;

  free (wrk);
}

static unsigned int
etp_nreqs (void)
{
  int retval;
  if (WORDACCESS_UNSAFE) X_LOCK   (reqlock);
  retval = nreqs;
  if (WORDACCESS_UNSAFE) X_UNLOCK (reqlock);
  return retval;
}

static unsigned int
etp_nready (void)
{
  unsigned int retval;

  if (WORDACCESS_UNSAFE) X_LOCK   (reqlock);
  retval = nready;
  if (WORDACCESS_UNSAFE) X_UNLOCK (reqlock);

  return retval;
}

static unsigned int
etp_npending (void)
{
  unsigned int retval;

  if (WORDACCESS_UNSAFE) X_LOCK   (reqlock);
  retval = npending;
  if (WORDACCESS_UNSAFE) X_UNLOCK (reqlock);

  return retval;
}

static unsigned int
etp_nthreads (void)
{
  unsigned int retval;

  if (WORDACCESS_UNSAFE) X_LOCK   (reqlock);
  retval = started;
  if (WORDACCESS_UNSAFE) X_UNLOCK (reqlock);

  return retval;
}

/*
 * a somewhat faster data structure might be nice, but
 * with 8 priorities this actually needs <20 insns
 * per shift, the most expensive operation.
 */
typedef struct {
  ETP_REQ *qs[ETP_NUM_PRI], *qe[ETP_NUM_PRI]; /* qstart, qend */
  int size;
} etp_reqq;

static etp_reqq req_queue;
static etp_reqq res_queue;

static void ecb_noinline ecb_cold
reqq_init (etp_reqq *q)
{
  int pri;

  for (pri = 0; pri < ETP_NUM_PRI; ++pri)
    q->qs[pri] = q->qe[pri] = 0;

  q->size = 0;
}

static int ecb_noinline
reqq_push (etp_reqq *q, ETP_REQ *req)
{
  int pri = req->pri;
  req->next = 0;

  if (q->qe[pri])
    {
      q->qe[pri]->next = req;
      q->qe[pri] = req;
    }
  else
    q->qe[pri] = q->qs[pri] = req;

  return q->size++;
}

static ETP_REQ * ecb_noinline
reqq_shift (etp_reqq *q)
{
  int pri;

  if (!q->size)
    return 0;

  --q->size;

  for (pri = ETP_NUM_PRI; pri--; )
    {
      eio_req *req = q->qs[pri];

      if (req)
        {
          if (!(q->qs[pri] = (eio_req *)req->next))
            q->qe[pri] = 0;

          return req;
        }
    }

  abort ();
}

static int ecb_cold
etp_init (void (*want_poll)(void), void (*done_poll)(void))
{
  X_MUTEX_CREATE (wrklock);
  X_MUTEX_CREATE (reslock);
  X_MUTEX_CREATE (reqlock);
  X_COND_CREATE  (reqwait);

  reqq_init (&req_queue);
  reqq_init (&res_queue);

  wrk_first.next =
  wrk_first.prev = &wrk_first;

  started  = 0;
  idle     = 0;
  nreqs    = 0;
  nready   = 0;
  npending = 0;

  want_poll_cb = want_poll;
  done_poll_cb = done_poll;

  return 0;
}

X_THREAD_PROC (etp_proc);

static void ecb_cold
etp_start_thread (void)
{
  etp_worker *wrk = calloc (1, sizeof (etp_worker));

  /*TODO*/
  assert (("unable to allocate worker thread data", wrk));

  X_LOCK (wrklock);

  if (thread_create (&wrk->tid, etp_proc, (void *)wrk))
    {
      wrk->prev = &wrk_first;
      wrk->next = wrk_first.next;
      wrk_first.next->prev = wrk;
      wrk_first.next = wrk;
      ++started;
    }
  else
    free (wrk);

  X_UNLOCK (wrklock);
}

static void
etp_maybe_start_thread (void)
{
  if (ecb_expect_true (etp_nthreads () >= wanted))
    return;
  
  /* todo: maybe use idle here, but might be less exact */
  if (ecb_expect_true (0 <= (int)etp_nthreads () + (int)etp_npending () - (int)etp_nreqs ()))
    return;

  etp_start_thread ();
}

static void ecb_cold
etp_end_thread (void)
{
  eio_req *req = calloc (1, sizeof (eio_req));

  req->type = -1;
  req->pri  = ETP_PRI_MAX - ETP_PRI_MIN;

  X_LOCK (reqlock);
  reqq_push (&req_queue, req);
  X_COND_SIGNAL (reqwait);
  X_UNLOCK (reqlock);

  X_LOCK (wrklock);
  --started;
  X_UNLOCK (wrklock);
}

static int
etp_poll (void)
{
  unsigned int maxreqs;
  unsigned int maxtime;
  struct timeval tv_start, tv_now;

  X_LOCK (reslock);
  maxreqs = max_poll_reqs;
  maxtime = max_poll_time;
  X_UNLOCK (reslock);

  if (maxtime)
    gettimeofday (&tv_start, 0);

  for (;;)
    {
      ETP_REQ *req;

      etp_maybe_start_thread ();

      X_LOCK (reslock);
      req = reqq_shift (&res_queue);

      if (req)
        {
          --npending;

          if (!res_queue.size && done_poll_cb)
            done_poll_cb ();
        }

      X_UNLOCK (reslock);

      if (!req)
        return 0;

      X_LOCK (reqlock);
      --nreqs;
      X_UNLOCK (reqlock);

      if (ecb_expect_false (req->type == EIO_GROUP && req->size))
        {
          req->int1 = 1; /* mark request as delayed */
          continue;
        }
      else
        {
          int res = ETP_FINISH (req);
          if (ecb_expect_false (res))
            return res;
        }

      if (ecb_expect_false (maxreqs && !--maxreqs))
        break;

      if (maxtime)
        {
          gettimeofday (&tv_now, 0);

          if (tvdiff (&tv_start, &tv_now) >= maxtime)
            break;
        }
    }

  errno = EAGAIN;
  return -1;
}

static void
etp_cancel (ETP_REQ *req)
{
  req->cancelled = 1;

  eio_grp_cancel (req);
}

static void
etp_submit (ETP_REQ *req)
{
  req->pri -= ETP_PRI_MIN;

  if (ecb_expect_false (req->pri < ETP_PRI_MIN - ETP_PRI_MIN)) req->pri = ETP_PRI_MIN - ETP_PRI_MIN;
  if (ecb_expect_false (req->pri > ETP_PRI_MAX - ETP_PRI_MIN)) req->pri = ETP_PRI_MAX - ETP_PRI_MIN;

  if (ecb_expect_false (req->type == EIO_GROUP))
    {
      /* I hope this is worth it :/ */
      X_LOCK (reqlock);
      ++nreqs;
      X_UNLOCK (reqlock);

      X_LOCK (reslock);

      ++npending;

      if (!reqq_push (&res_queue, req) && want_poll_cb)
        want_poll_cb ();

      X_UNLOCK (reslock);
    }
  else
    {
      X_LOCK (reqlock);
      ++nreqs;
      ++nready;
      reqq_push (&req_queue, req);
      X_COND_SIGNAL (reqwait);
      X_UNLOCK (reqlock);

      etp_maybe_start_thread ();
    }
}

static void ecb_cold
etp_set_max_poll_time (double nseconds)
{
  if (WORDACCESS_UNSAFE) X_LOCK   (reslock);
  max_poll_time = nseconds * EIO_TICKS;
  if (WORDACCESS_UNSAFE) X_UNLOCK (reslock);
}

static void ecb_cold
etp_set_max_poll_reqs (unsigned int maxreqs)
{
  if (WORDACCESS_UNSAFE) X_LOCK   (reslock);
  max_poll_reqs = maxreqs;
  if (WORDACCESS_UNSAFE) X_UNLOCK (reslock);
}

static void ecb_cold
etp_set_max_idle (unsigned int nthreads)
{
  if (WORDACCESS_UNSAFE) X_LOCK   (reqlock);
  max_idle = nthreads;
  if (WORDACCESS_UNSAFE) X_UNLOCK (reqlock);
}

static void ecb_cold
etp_set_idle_timeout (unsigned int seconds)
{
  if (WORDACCESS_UNSAFE) X_LOCK   (reqlock);
  idle_timeout = seconds;
  if (WORDACCESS_UNSAFE) X_UNLOCK (reqlock);
}

static void ecb_cold
etp_set_min_parallel (unsigned int nthreads)
{
  if (wanted < nthreads)
    wanted = nthreads;
}

static void ecb_cold
etp_set_max_parallel (unsigned int nthreads)
{
  if (wanted > nthreads)
    wanted = nthreads;

  while (started > wanted)
    etp_end_thread ();
}

/*****************************************************************************/

static void
grp_try_feed (eio_req *grp)
{
  while (grp->size < grp->int2 && !EIO_CANCELLED (grp))
    {
      grp->flags &= ~EIO_FLAG_GROUPADD;

      EIO_FEED (grp);

      /* stop if no progress has been made */
      if (!(grp->flags & EIO_FLAG_GROUPADD))
        {
          grp->feed = 0;
          break;
        }
    }
}

static int
grp_dec (eio_req *grp)
{
  --grp->size;

  /* call feeder, if applicable */
  grp_try_feed (grp);

  /* finish, if done */
  if (!grp->size && grp->int1)
    return eio_finish (grp);
  else
    return 0;
}

static void
eio_destroy (eio_req *req)
{
  if ((req)->flags & EIO_FLAG_PTR1_FREE) free (req->ptr1);
  if ((req)->flags & EIO_FLAG_PTR2_FREE) free (req->ptr2);

  EIO_DESTROY (req);
}

static int
eio_finish (eio_req *req)
{
  int res = EIO_FINISH (req);

  if (req->grp)
    {
      int res2;
      eio_req *grp = req->grp;

      /* unlink request */
      if (req->grp_next) req->grp_next->grp_prev = req->grp_prev;
      if (req->grp_prev) req->grp_prev->grp_next = req->grp_next;

      if (grp->grp_first == req)
        grp->grp_first = req->grp_next;

      res2 = grp_dec (grp);

      if (!res)
        res = res2;
    }

  eio_destroy (req);

  return res;
}

void
eio_grp_cancel (eio_req *grp)
{
  for (grp = grp->grp_first; grp; grp = grp->grp_next)
    eio_cancel (grp);
}

void
eio_cancel (eio_req *req)
{
  etp_cancel (req);
}

void
eio_submit (eio_req *req)
{
  etp_submit (req);
}

unsigned int
eio_nreqs (void)
{
  return etp_nreqs ();
}

unsigned int
eio_nready (void)
{
  return etp_nready ();
}

unsigned int
eio_npending (void)
{
  return etp_npending ();
}

unsigned int ecb_cold
eio_nthreads (void)
{
  return etp_nthreads ();
}

void ecb_cold
eio_set_max_poll_time (double nseconds)
{
  etp_set_max_poll_time (nseconds);
}

void ecb_cold
eio_set_max_poll_reqs (unsigned int maxreqs)
{
  etp_set_max_poll_reqs (maxreqs);
}

void ecb_cold
eio_set_max_idle (unsigned int nthreads)
{
  etp_set_max_idle (nthreads);
}

void ecb_cold
eio_set_idle_timeout (unsigned int seconds)
{
  etp_set_idle_timeout (seconds);
}

void ecb_cold
eio_set_min_parallel (unsigned int nthreads)
{
  etp_set_min_parallel (nthreads);
}

void ecb_cold
eio_set_max_parallel (unsigned int nthreads)
{
  etp_set_max_parallel (nthreads);
}

int eio_poll (void)
{
  return etp_poll ();
}

/*****************************************************************************/
/* work around various missing functions */

#if !HAVE_PREADWRITE
# undef pread
# undef pwrite
# define pread  eio__pread
# define pwrite eio__pwrite

eio_ssize_t
eio__pread (int fd, void *buf, size_t count, off_t offset)
{
  eio_ssize_t res;
  off_t ooffset;

  X_LOCK (preadwritelock);
  ooffset = lseek (fd, 0, SEEK_CUR);
  lseek (fd, offset, SEEK_SET);
  res = read (fd, buf, count);
  lseek (fd, ooffset, SEEK_SET);
  X_UNLOCK (preadwritelock);

  return res;
}

eio_ssize_t
eio__pwrite (int fd, void *buf, size_t count, off_t offset)
{
  eio_ssize_t res;
  off_t ooffset;

  X_LOCK (preadwritelock);
  ooffset = lseek (fd, 0, SEEK_CUR);
  lseek (fd, offset, SEEK_SET);
  res = write (fd, buf, count);
  lseek (fd, ooffset, SEEK_SET);
  X_UNLOCK (preadwritelock);

  return res;
}
#endif

#ifndef HAVE_UTIMES

# undef utimes
# define utimes(path,times)  eio__utimes (path, times)

static int
eio__utimes (const char *filename, const struct timeval times[2])
{
  if (times)
    {
      struct utimbuf buf;

      buf.actime  = times[0].tv_sec;
      buf.modtime = times[1].tv_sec;

      return utime (filename, &buf);
    }
  else
    return utime (filename, 0);
}

#endif

#ifndef HAVE_FUTIMES

# undef futimes
# define futimes(fd,times) eio__futimes (fd, times)

static int
eio__futimes (int fd, const struct timeval tv[2])
{
  errno = ENOSYS;
  return -1;
}

#endif

#if !HAVE_FDATASYNC
# undef fdatasync
# define fdatasync(fd) fsync (fd)
#endif

static int
eio__syncfs (int fd)
{
  int res;

#if HAVE_SYS_SYNCFS
  res = (int)syscall (__NR_syncfs, (int)(fd));
#else
  res = -1;
  errno = ENOSYS;
#endif

  if (res < 0 && errno == ENOSYS && fd >= 0)
    sync ();

  return res;
}

/* sync_file_range always needs emulation */
static int
eio__sync_file_range (int fd, off_t offset, size_t nbytes, unsigned int flags)
{
#if HAVE_SYNC_FILE_RANGE
  int res;

  if (EIO_SYNC_FILE_RANGE_WAIT_BEFORE   != SYNC_FILE_RANGE_WAIT_BEFORE
      || EIO_SYNC_FILE_RANGE_WRITE      != SYNC_FILE_RANGE_WRITE
      || EIO_SYNC_FILE_RANGE_WAIT_AFTER != SYNC_FILE_RANGE_WAIT_AFTER)
    {
      flags = 0
         | (flags & EIO_SYNC_FILE_RANGE_WAIT_BEFORE ? SYNC_FILE_RANGE_WAIT_BEFORE : 0)
         | (flags & EIO_SYNC_FILE_RANGE_WRITE       ? SYNC_FILE_RANGE_WRITE       : 0)
         | (flags & EIO_SYNC_FILE_RANGE_WAIT_AFTER  ? SYNC_FILE_RANGE_WAIT_AFTER  : 0);
    }

  res = sync_file_range (fd, offset, nbytes, flags);

  if (!res || errno != ENOSYS)
    return res;
#endif

  /* even though we could play tricks with the flags, it's better to always
   * call fdatasync, as that matches the expectation of its users best */
  return fdatasync (fd);
}

static int
eio__fallocate (int fd, int mode, off_t offset, size_t len)
{
#if HAVE_FALLOCATE
  return fallocate (fd, mode, offset, len);
#else
  errno = ENOSYS;
  return -1;
#endif
}

#if !HAVE_READAHEAD
# undef readahead
# define readahead(fd,offset,count) eio__readahead (fd, offset, count, self)

static eio_ssize_t
eio__readahead (int fd, off_t offset, size_t count, etp_worker *self)
{
  size_t todo = count;
  dBUF;

  while (todo > 0)
    {
      size_t len = todo < EIO_BUFSIZE ? todo : EIO_BUFSIZE;

      pread (fd, eio_buf, len, offset);
      offset += len;
      todo   -= len;
    }

  FUBd;

  errno = 0;
  return count;
}

#endif

/* sendfile always needs emulation */
static eio_ssize_t
eio__sendfile (int ofd, int ifd, off_t offset, size_t count)
{
  eio_ssize_t written = 0;
  eio_ssize_t res;

  if (!count)
    return 0;

  for (;;)
    {
#ifdef __APPLE__
# undef HAVE_SENDFILE /* broken, as everything on os x */
#endif
#if HAVE_SENDFILE
# if __linux
      off_t soffset = offset;
      res = sendfile (ofd, ifd, &soffset, count);

# elif __FreeBSD__
      /*
       * Of course, the freebsd sendfile is a dire hack with no thoughts
       * wasted on making it similar to other I/O functions.
       */
      off_t sbytes;
      res = sendfile (ifd, ofd, offset, count, 0, &sbytes, 0);

      #if 0 /* according to the manpage, this is correct, but broken behaviour */
      /* freebsd' sendfile will return 0 on success */
      /* freebsd 8 documents it as only setting *sbytes on EINTR and EAGAIN, but */
      /* not on e.g. EIO or EPIPE - sounds broken */
      if ((res < 0 && (errno == EAGAIN || errno == EINTR) && sbytes) || res == 0)
        res = sbytes;
      #endif

      /* according to source inspection, this is correct, and useful behaviour */
      if (sbytes)
        res = sbytes;

# elif defined (__APPLE__)
      off_t sbytes = count;
      res = sendfile (ifd, ofd, offset, &sbytes, 0, 0);

      /* according to the manpage, sbytes is always valid */
      if (sbytes)
        res = sbytes;

# elif __hpux
      res = sendfile (ofd, ifd, offset, count, 0, 0);

# elif __solaris
      struct sendfilevec vec;
      size_t sbytes;

      vec.sfv_fd   = ifd;
      vec.sfv_flag = 0;
      vec.sfv_off  = offset;
      vec.sfv_len  = count;

      res = sendfilev (ofd, &vec, 1, &sbytes);

      if (res < 0 && sbytes)
        res = sbytes;

# endif

#elif defined (_WIN32) && 0
      /* does not work, just for documentation of what would need to be done */
      /* actually, cannot be done like this, as TransmitFile changes the file offset, */
      /* libeio guarantees that the file offset does not change, and windows */
      /* has no way to get an independent handle to the same file description */
      HANDLE h = TO_SOCKET (ifd);
      SetFilePointer (h, offset, 0, FILE_BEGIN);
      res = TransmitFile (TO_SOCKET (ofd), h, count, 0, 0, 0, 0);

#else
      res = -1;
      errno = ENOSYS;
#endif

      /* we assume sendfile can copy at least 128mb in one go */
      if (res <= 128 * 1024 * 1024)
        {
          if (res > 0)
            written += res;

          if (written)
            return written;

          break;
        }
      else
        {
          /* if we requested more, then probably the kernel was lazy */
          written += res;
          offset  += res;
          count   -= res;

          if (!count)
            return written;
        }
    }

  if (res < 0
      && (errno == ENOSYS || errno == EINVAL || errno == ENOTSOCK
          /* BSDs */
#ifdef ENOTSUP /* sigh, if the steenking pile called openbsd would only try to at least compile posix code... */
          || errno == ENOTSUP
#endif
#ifdef EOPNOTSUPP /* windows */
          || errno == EOPNOTSUPP /* BSDs */
#endif
#if __solaris
          || errno == EAFNOSUPPORT || errno == EPROTOTYPE
#endif
         )
      )
    {
      /* emulate sendfile. this is a major pain in the ass */
      dBUF;

      res = 0;

      while (count)
        {
          eio_ssize_t cnt;
          
          cnt = pread (ifd, eio_buf, count > EIO_BUFSIZE ? EIO_BUFSIZE : count, offset);

          if (cnt <= 0)
            {
              if (cnt && !res) res = -1;
              break;
            }

          cnt = write (ofd, eio_buf, cnt);

          if (cnt <= 0)
            {
              if (cnt && !res) res = -1;
              break;
            }

          offset += cnt;
          res    += cnt;
          count  -= cnt;
        }

      FUBd;
    }

  return res;
}

#ifdef PAGESIZE
# define eio_pagesize() PAGESIZE
#else
static intptr_t
eio_pagesize (void)
{
  static intptr_t page;

  if (!page)
    page = sysconf (_SC_PAGESIZE);

  return page;
}
#endif

static void
eio_page_align (void **addr, size_t *length)
{
  intptr_t mask = eio_pagesize () - 1;

  /* round down addr */
  intptr_t adj = mask & (intptr_t)*addr;

  *addr   = (void *)((intptr_t)*addr - adj);
  *length += adj;

  /* round up length */
  *length = (*length + mask) & ~mask;
}

#if !_POSIX_MEMLOCK
# define eio__mlockall(a) EIO_ENOSYS ()
#else

static int
eio__mlockall (int flags)
{
  #if __GLIBC__ == 2 && __GLIBC_MINOR__ <= 7
    extern int mallopt (int, int);
    mallopt (-6, 238); /* http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=473812 */
  #endif

  if (EIO_MCL_CURRENT   != MCL_CURRENT
      || EIO_MCL_FUTURE != MCL_FUTURE)
    {
      flags = 0
         | (flags & EIO_MCL_CURRENT ? MCL_CURRENT : 0)
         | (flags & EIO_MCL_FUTURE  ? MCL_FUTURE : 0);
    }

  return mlockall (flags);
}
#endif

#if !_POSIX_MEMLOCK_RANGE
# define eio__mlock(a,b) EIO_ENOSYS ()
#else

static int
eio__mlock (void *addr, size_t length)
{
  eio_page_align (&addr, &length);

  return mlock (addr, length);
}

#endif

#if !(_POSIX_MAPPED_FILES && _POSIX_SYNCHRONIZED_IO)
# define eio__msync(a,b,c) EIO_ENOSYS ()
#else

static int
eio__msync (void *mem, size_t len, int flags)
{
  eio_page_align (&mem, &len);

  if (EIO_MS_ASYNC         != MS_SYNC
      || EIO_MS_INVALIDATE != MS_INVALIDATE
      || EIO_MS_SYNC       != MS_SYNC)
    {
      flags = 0
         | (flags & EIO_MS_ASYNC      ? MS_ASYNC : 0)
         | (flags & EIO_MS_INVALIDATE ? MS_INVALIDATE : 0)
         | (flags & EIO_MS_SYNC       ? MS_SYNC : 0);
    }

  return msync (mem, len, flags);
}

#endif

static int
eio__mtouch (eio_req *req)
{
  void *mem  = req->ptr2;
  size_t len = req->size;
  int flags  = req->int1;

  eio_page_align (&mem, &len);

  {
    intptr_t addr = (intptr_t)mem;
    intptr_t end = addr + len;
    intptr_t page = eio_pagesize ();

    if (addr < end)
      if (flags & EIO_MT_MODIFY) /* modify */
        do { *((volatile sig_atomic_t *)addr) |= 0; } while ((addr += page) < len && !EIO_CANCELLED (req));
      else
        do { *((volatile sig_atomic_t *)addr)     ; } while ((addr += page) < len && !EIO_CANCELLED (req));
  }

  return 0;
}

/*****************************************************************************/
/* requests implemented outside eio_execute, because they are so large */

static void
eio__realpath (eio_req *req, etp_worker *self)
{
  char *rel = req->ptr1;
  char *res;
  char *tmp1, *tmp2;
#if SYMLOOP_MAX > 32
  int symlinks = SYMLOOP_MAX;
#else
  int symlinks = 32;
#endif

  req->result = -1;

  errno = EINVAL;
  if (!rel)
    return;

  errno = ENOENT;
  if (!*rel)
    return;

  if (!req->ptr2)
    {
      X_LOCK (wrklock);
      req->flags |= EIO_FLAG_PTR2_FREE;
      X_UNLOCK (wrklock);
      req->ptr2 = malloc (PATH_MAX * 3);

      errno = ENOMEM;
      if (!req->ptr2)
        return;
    }

  res  = req->ptr2;
  tmp1 = res  + PATH_MAX;
  tmp2 = tmp1 + PATH_MAX;

#if 0 /* disabled, the musl way to do things is just too racy */
#if __linux && defined(O_NONBLOCK) && defined(O_NOATIME)
  /* on linux we may be able to ask the kernel */
  {
    int fd = open (rel, O_RDONLY | O_NONBLOCK | O_NOCTTY | O_NOATIME);

    if (fd >= 0)
      {
        sprintf (tmp1, "/proc/self/fd/%d", fd);
        req->result = readlink (tmp1, res, PATH_MAX);
        close (fd);

        /* here we should probably stat the open file and the disk file, to make sure they still match */

        if (req->result > 0)
          goto done;
      }
    else if (errno == ELOOP || errno == ENAMETOOLONG || errno == ENOENT || errno == ENOTDIR || errno == EIO)
      return;
  }
#endif
#endif

  if (*rel != '/')
    {
      if (!getcwd (res, PATH_MAX))
        return;

      if (res [1]) /* only use if not / */
        res += strlen (res);
    }

  while (*rel)
    {
      eio_ssize_t len, linklen;
      char *beg = rel;

      while (*rel && *rel != '/')
        ++rel;

      len = rel - beg;

      if (!len) /* skip slashes */
        {
          ++rel;
          continue;
        }

      if (beg [0] == '.')
        {
          if (len == 1)
            continue; /* . - nop */

          if (beg [1] == '.' && len == 2)
            {
              /* .. - back up one component, if possible */

              while (res != req->ptr2)
                if (*--res == '/')
                  break;

              continue;
            }
        }

        errno = ENAMETOOLONG;
        if (res + 1 + len + 1 >= tmp1)
          return;

        /* copy one component */
        *res = '/';
        memcpy (res + 1, beg, len);

        /* zero-terminate, for readlink */
        res [len + 1] = 0;

        /* now check if it's a symlink */
        linklen = readlink (req->ptr2, tmp1, PATH_MAX);

        if (linklen < 0)
          {
            if (errno != EINVAL)
              return;

            /* it's a normal directory. hopefully */
            res += len + 1;
          }
        else
          {
            /* yay, it was a symlink - build new path in tmp2 */
            int rellen = strlen (rel);

            errno = ENAMETOOLONG;
            if (linklen + 1 + rellen >= PATH_MAX)
              return;

            errno = ELOOP;
            if (!--symlinks)
              return;

            if (*tmp1 == '/')
              res = req->ptr2; /* symlink resolves to an absolute path */

            /* we need to be careful, as rel might point into tmp2 already */
            memmove (tmp2 + linklen + 1, rel, rellen + 1);
            tmp2 [linklen] = '/';
            memcpy (tmp2, tmp1, linklen);

            rel = tmp2;
          }
    }

  /* special case for the lone root path */
  if (res == req->ptr2)
    *res++ = '/';

  req->result = res - (char *)req->ptr2;

done:
  req->ptr2 = realloc (req->ptr2, req->result); /* trade time for space savings */
}

static signed char
eio_dent_cmp (const eio_dirent *a, const eio_dirent *b)
{
  return a->score - b->score ? a->score - b->score /* works because our signed char is always 0..100 */
       : a->inode < b->inode ? -1
       : a->inode > b->inode ?  1
       :                        0;
}

#define EIO_DENT_CMP(i,op,j) eio_dent_cmp (&i, &j) op 0

#define EIO_SORT_CUTOFF 30 /* quite high, but performs well on many filesystems */
#define EIO_SORT_FAST   60 /* when to only use insertion sort */

static void
eio_dent_radix_sort (eio_dirent *dents, int size, signed char score_bits, eio_ino_t inode_bits)
{
  unsigned char bits [9 + sizeof (eio_ino_t) * 8];
  unsigned char *bit = bits;

  assert (CHAR_BIT == 8);
  assert (sizeof (eio_dirent) * 8 < 256);
  assert (offsetof (eio_dirent, inode)); /* we use bit #0 as sentinel */
  assert (offsetof (eio_dirent, score)); /* we use bit #0 as sentinel */

  if (size <= EIO_SORT_FAST)
    return;

  /* first prepare an array of bits to test in our radix sort */
  /* try to take endianness into account, as well as differences in eio_ino_t sizes */
  /* inode_bits must contain all inodes ORed together */
  /* which is used to skip bits that are 0 everywhere, which is very common */
  {
    eio_ino_t endianness;
    int i, j;

    /* we store the byte offset of byte n into byte n of "endianness" */
    for (i = 0; i < sizeof (eio_ino_t); ++i)
      ((unsigned char *)&endianness)[i] = i;

    *bit++ = 0;

    for (i = 0; i < sizeof (eio_ino_t); ++i)
      {
        /* shifting off the byte offsets out of "endianness" */
        int offs = (offsetof (eio_dirent, inode) + (endianness & 0xff)) * 8;
        endianness >>= 8;

        for (j = 0; j < 8; ++j)
          if (inode_bits & (((eio_ino_t)1) << (i * 8 + j)))
            *bit++ = offs + j;
      }

    for (j = 0; j < 8; ++j)
      if (score_bits & (1 << j))
        *bit++ = offsetof (eio_dirent, score) * 8 + j;
  }

  /* now actually do the sorting (a variant of MSD radix sort) */
  {
    eio_dirent    *base_stk [9 + sizeof (eio_ino_t) * 8], *base;
    eio_dirent    *end_stk  [9 + sizeof (eio_ino_t) * 8], *end;
    unsigned char *bit_stk  [9 + sizeof (eio_ino_t) * 8];
    int stk_idx = 0;

    base_stk [stk_idx] = dents;
    end_stk  [stk_idx] = dents + size;
    bit_stk  [stk_idx] = bit - 1;

    do
      {
        base = base_stk [stk_idx];
        end  = end_stk  [stk_idx];
        bit  = bit_stk  [stk_idx];

        for (;;)
          {
            unsigned char O = *bit >> 3;
            unsigned char M = 1 << (*bit & 7);

            eio_dirent *a = base;
            eio_dirent *b = end;

            if (b - a < EIO_SORT_CUTOFF)
              break;

            /* now bit-partition the array on the bit */
            /* this ugly asymmetric loop seems to perform much better than typical */
            /* partition algos found in the literature */
            do
              if (!(((unsigned char *)a)[O] & M))
                ++a;
              else if (!(((unsigned char *)--b)[O] & M))
                {
                  eio_dirent tmp = *a; *a = *b; *b = tmp;
                  ++a;
                }
            while (b > a);

            /* next bit, or stop, if no bits left in this path */
            if (!*--bit)
              break;

            base_stk [stk_idx] = a;
            end_stk  [stk_idx] = end;
            bit_stk  [stk_idx] = bit;
            ++stk_idx;

            end = a;
          }
      }
    while (stk_idx--);
  }
}

static void
eio_dent_insertion_sort (eio_dirent *dents, int size)
{
  /* first move the smallest element to the front, to act as a sentinel */
  {
    int i;
    eio_dirent *min = dents;
    
    /* the radix pre-pass ensures that the minimum element is in the first EIO_SORT_CUTOFF + 1 elements */
    for (i = size > EIO_SORT_FAST ? EIO_SORT_CUTOFF + 1 : size; --i; )
      if (EIO_DENT_CMP (dents [i], <, *min))
        min = &dents [i];

    /* swap elements 0 and j (minimum) */
    {
      eio_dirent tmp = *dents; *dents = *min; *min = tmp;
    }
  }

  /* then do standard insertion sort, assuming that all elements are >= dents [0] */
  {
    eio_dirent *i, *j;

    for (i = dents + 1; i < dents + size; ++i)
      {
        eio_dirent value = *i;

        for (j = i - 1; EIO_DENT_CMP (*j, >, value); --j)
          j [1] = j [0];

        j [1] = value;
      }
  }
}

static void
eio_dent_sort (eio_dirent *dents, int size, signed char score_bits, eio_ino_t inode_bits)
{
  if (size <= 1)
    return; /* our insertion sort relies on size > 0 */

  /* first we use a radix sort, but only for dirs >= EIO_SORT_FAST */
  /* and stop sorting when the partitions are <= EIO_SORT_CUTOFF */
  eio_dent_radix_sort (dents, size, score_bits, inode_bits);

  /* use an insertion sort at the end, or for small arrays, */
  /* as insertion sort is more efficient for small partitions */
  eio_dent_insertion_sort (dents, size);
}

/* read a full directory */
static void
eio__scandir (eio_req *req, etp_worker *self)
{
  char *name, *names;
  int namesalloc = 4096 - sizeof (void *) * 4;
  int namesoffs = 0;
  int flags = req->int1;
  eio_dirent *dents = 0;
  int dentalloc = 128;
  int dentoffs = 0;
  eio_ino_t inode_bits = 0;
#ifdef _WIN32
  HANDLE dirp;
  WIN32_FIND_DATA entp;
#else
  DIR *dirp;
  EIO_STRUCT_DIRENT *entp;
#endif

  req->result = -1;

  if (!(flags & EIO_READDIR_DENTS))
    flags &= ~(EIO_READDIR_DIRS_FIRST | EIO_READDIR_STAT_ORDER);

#ifdef _WIN32
  {
    int len = strlen ((const char *)req->ptr1);
    char *path = malloc (MAX_PATH);
    const char *fmt;

    if (!len)
      fmt = "./*";
    else if (((const char *)req->ptr1)[len - 1] == '/' || ((const char *)req->ptr1)[len - 1] == '\\')
      fmt = "%s*";
    else
      fmt = "%s/*";

    _snprintf (path, MAX_PATH, fmt, (const char *)req->ptr1);
    dirp = FindFirstFile (path, &entp);
    free (path);

    if (dirp == INVALID_HANDLE_VALUE)
     {
       dirp = 0;

        /* should steal _dosmaperr */
        switch (GetLastError ())
          {
            case ERROR_FILE_NOT_FOUND:
              req->result = 0;
              break;

            case ERROR_INVALID_NAME:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_NO_MORE_FILES:
              errno = ENOENT;
              break;

            case ERROR_NOT_ENOUGH_MEMORY:
              errno = ENOMEM;
              break;

            default:
              errno = EINVAL;
              break;
          }
     }
  }
#else
  dirp = opendir (req->ptr1);
#endif

  if (req->flags & EIO_FLAG_PTR1_FREE)
    free (req->ptr1);

  req->flags |= EIO_FLAG_PTR1_FREE | EIO_FLAG_PTR2_FREE;
  req->ptr1 = dents = flags ? malloc (dentalloc * sizeof (eio_dirent)) : 0;
  req->ptr2 = names = malloc (namesalloc);

  if (dirp && names && (!flags || dents))
    for (;;)
      {
        int done;

#ifdef _WIN32
        done = !dirp;
#else
        errno = 0;
        entp = readdir (dirp);
        done = !entp;
#endif

        if (done)
          {
#ifndef _WIN32
            int old_errno = errno;
            closedir (dirp);
            errno = old_errno;

            if (errno)
              break;
#endif

            /* sort etc. */
            req->int1   = flags;
            req->result = dentoffs;

            if (flags & EIO_READDIR_STAT_ORDER)
              eio_dent_sort (dents, dentoffs, flags & EIO_READDIR_DIRS_FIRST ? 7 : 0, inode_bits);
            else if (flags & EIO_READDIR_DIRS_FIRST)
              if (flags & EIO_READDIR_FOUND_UNKNOWN)
                eio_dent_sort (dents, dentoffs, 7, inode_bits); /* sort by score and inode */
              else
                {
                  /* in this case, all is known, and we just put dirs first and sort them */
                  eio_dirent *oth = dents + dentoffs;
                  eio_dirent *dir = dents;

                  /* now partition dirs to the front, and non-dirs to the back */
                  /* by walking from both sides and swapping if necessary */
                  while (oth > dir)
                    {
                      if (dir->type == EIO_DT_DIR)
                        ++dir;
                      else if ((--oth)->type == EIO_DT_DIR)
                        {
                          eio_dirent tmp = *dir; *dir = *oth; *oth = tmp;

                          ++dir;
                        }
                    }

                  /* now sort the dirs only (dirs all have the same score) */
                  eio_dent_sort (dents, dir - dents, 0, inode_bits);
                }

            break;
          }

        /* now add the entry to our list(s) */
        name = D_NAME (entp);

        /* skip . and .. entries */
        if (name [0] != '.' || (name [1] && (name [1] != '.' || name [2])))
          {
            int len = D_NAMLEN (entp) + 1;

            while (ecb_expect_false (namesoffs + len > namesalloc))
              {
                namesalloc *= 2;
                req->ptr2 = names = realloc (names, namesalloc);

                if (!names)
                  break;
              }

            memcpy (names + namesoffs, name, len);

            if (dents)
              {
                struct eio_dirent *ent;

                if (ecb_expect_false (dentoffs == dentalloc))
                  {
                    dentalloc *= 2;
                    req->ptr1 = dents = realloc (dents, dentalloc * sizeof (eio_dirent));

                    if (!dents)
                      break;
                  }

                ent = dents + dentoffs;

                ent->nameofs = namesoffs; /* rather dirtily we store the offset in the pointer */
                ent->namelen = len - 1;
                ent->inode   = D_INO (entp);

                inode_bits |= ent->inode;

                switch (D_TYPE (entp))
                  {
                    default:
                      ent->type = EIO_DT_UNKNOWN;
                      flags |= EIO_READDIR_FOUND_UNKNOWN;
                      break;

                    #ifdef DT_FIFO
                      case DT_FIFO: ent->type = EIO_DT_FIFO; break;
                    #endif
                    #ifdef DT_CHR
                      case DT_CHR:  ent->type = EIO_DT_CHR;  break;
                    #endif          
                    #ifdef DT_MPC
                      case DT_MPC:  ent->type = EIO_DT_MPC;  break;
                    #endif          
                    #ifdef DT_DIR
                      case DT_DIR:  ent->type = EIO_DT_DIR;  break;
                    #endif          
                    #ifdef DT_NAM
                      case DT_NAM:  ent->type = EIO_DT_NAM;  break;
                    #endif          
                    #ifdef DT_BLK
                      case DT_BLK:  ent->type = EIO_DT_BLK;  break;
                    #endif          
                    #ifdef DT_MPB
                      case DT_MPB:  ent->type = EIO_DT_MPB;  break;
                    #endif          
                    #ifdef DT_REG
                      case DT_REG:  ent->type = EIO_DT_REG;  break;
                    #endif          
                    #ifdef DT_NWK
                      case DT_NWK:  ent->type = EIO_DT_NWK;  break;
                    #endif          
                    #ifdef DT_CMP
                      case DT_CMP:  ent->type = EIO_DT_CMP;  break;
                    #endif          
                    #ifdef DT_LNK
                      case DT_LNK:  ent->type = EIO_DT_LNK;  break;
                    #endif
                    #ifdef DT_SOCK
                      case DT_SOCK: ent->type = EIO_DT_SOCK; break;
                    #endif
                    #ifdef DT_DOOR
                      case DT_DOOR: ent->type = EIO_DT_DOOR; break;
                    #endif
                    #ifdef DT_WHT
                      case DT_WHT:  ent->type = EIO_DT_WHT;  break;
                    #endif
                  }

                ent->score = 7;

                if (flags & EIO_READDIR_DIRS_FIRST)
                  {
                    if (ent->type == EIO_DT_UNKNOWN)
                      {
                        if (*name == '.') /* leading dots are likely directories, and, in any case, rare */
                          ent->score = 1;
                        else if (!strchr (name, '.')) /* absense of dots indicate likely dirs */
                          ent->score = len <= 2 ? 4 - len : len <= 4 ? 4 : len <= 7 ? 5 : 6; /* shorter == more likely dir, but avoid too many classes */
                      }
                    else if (ent->type == EIO_DT_DIR)
                      ent->score = 0;
                  }
              }

            namesoffs += len;
            ++dentoffs;
          }

        if (EIO_CANCELLED (req))
          {
            errno = ECANCELED;
            break;
          }

#ifdef _WIN32
        if (!FindNextFile (dirp, &entp))
          {
            FindClose (dirp);
            dirp = 0;
          }
#endif
      }
}

/*****************************************************************************/

#define ALLOC(len)				\
  if (!req->ptr2)				\
    {						\
      X_LOCK (wrklock);				\
      req->flags |= EIO_FLAG_PTR2_FREE;		\
      X_UNLOCK (wrklock);			\
      req->ptr2 = malloc (len);			\
      if (!req->ptr2)				\
        {					\
          errno       = ENOMEM;			\
          req->result = -1;			\
          break;				\
        }					\
    }

X_THREAD_PROC (etp_proc)
{
  ETP_REQ *req;
  struct timespec ts;
  etp_worker *self = (etp_worker *)thr_arg;

#if HAVE_PRCTL_SET_NAME
  prctl (PR_SET_NAME, (unsigned long)"eio_thread", 0, 0, 0);
#endif

  /* try to distribute timeouts somewhat evenly */
  ts.tv_nsec = ((unsigned long)self & 1023UL) * (1000000000UL / 1024UL);

  for (;;)
    {
      ts.tv_sec = 0;

      X_LOCK (reqlock);

      for (;;)
        {
          self->req = req = reqq_shift (&req_queue);

          if (req)
            break;

          if (ts.tv_sec == 1) /* no request, but timeout detected, let's quit */
            {
              X_UNLOCK (reqlock);
              X_LOCK (wrklock);
              --started;
              X_UNLOCK (wrklock);
              goto quit;
            }

          ++idle;

          if (idle <= max_idle)
            /* we are allowed to idle, so do so without any timeout */
            X_COND_WAIT (reqwait, reqlock);
          else
            {
              /* initialise timeout once */
              if (!ts.tv_sec)
                ts.tv_sec = time (0) + idle_timeout;

              if (X_COND_TIMEDWAIT (reqwait, reqlock, ts) == ETIMEDOUT)
                ts.tv_sec = 1; /* assuming this is not a value computed above.,.. */
            }

          --idle;
        }

      --nready;

      X_UNLOCK (reqlock);
     
      if (req->type < 0)
        goto quit;

      ETP_EXECUTE (self, req);

      X_LOCK (reslock);

      ++npending;

      if (!reqq_push (&res_queue, req) && want_poll_cb)
        want_poll_cb ();

      self->req = 0;
      etp_worker_clear (self);

      X_UNLOCK (reslock);
    }

quit:
  X_LOCK (wrklock);
  etp_worker_free (self);
  X_UNLOCK (wrklock);

  return 0;
}

/*****************************************************************************/

int ecb_cold
eio_init (void (*want_poll)(void), void (*done_poll)(void))
{
#if !HAVE_PREADWRITE
  X_MUTEX_CREATE (preadwritelock);
#endif

  return etp_init (want_poll, done_poll);
}

ecb_inline void
eio_api_destroy (eio_req *req)
{
  free (req);
}

#define REQ(rtype)                                            	\
  eio_req *req;                                                 \
                                                                \
  req = (eio_req *)calloc (1, sizeof *req);                     \
  if (!req)                                                     \
    return 0;                                                   \
                                                                \
  req->type    = rtype;                                         \
  req->pri     = pri;						\
  req->finish  = cb;						\
  req->data    = data;						\
  req->destroy = eio_api_destroy;

#define SEND eio_submit (req); return req

#define PATH							\
  req->flags |= EIO_FLAG_PTR1_FREE;				\
  req->ptr1 = strdup (path);					\
  if (!req->ptr1)						\
    {								\
      eio_api_destroy (req);					\
      return 0;							\
    }

static void
eio_execute (etp_worker *self, eio_req *req)
{
  if (ecb_expect_false (EIO_CANCELLED (req)))
    {
      req->result  = -1;
      req->errorno = ECANCELED;
      return;
    }

  switch (req->type)
    {
      case EIO_READ:      ALLOC (req->size);
                          req->result = req->offs >= 0
                                      ? pread     (req->int1, req->ptr2, req->size, req->offs)
                                      : read      (req->int1, req->ptr2, req->size); break;

      case EIO_WRITE:
#if defined (__APPLE__)
                          pthread_mutex_lock (&apple_bug_writelock);
#endif

                          req->result = req->offs >= 0
                                      ? pwrite    (req->int1, req->ptr2, req->size, req->offs)
                                      : write     (req->int1, req->ptr2, req->size);

#if defined (__APPLE__)
                          pthread_mutex_unlock (&apple_bug_writelock);
#endif
                          break;

      case EIO_READAHEAD: req->result = readahead     (req->int1, req->offs, req->size); break;
      case EIO_SENDFILE:  req->result = eio__sendfile (req->int1, req->int2, req->offs, req->size); break;

      case EIO_STAT:      ALLOC (sizeof (EIO_STRUCT_STAT));
                          req->result = stat      (req->ptr1, (EIO_STRUCT_STAT *)req->ptr2); break;
      case EIO_LSTAT:     ALLOC (sizeof (EIO_STRUCT_STAT));
                          req->result = lstat     (req->ptr1, (EIO_STRUCT_STAT *)req->ptr2); break;
      case EIO_FSTAT:     ALLOC (sizeof (EIO_STRUCT_STAT));
                          req->result = fstat     (req->int1, (EIO_STRUCT_STAT *)req->ptr2); break;

      case EIO_STATVFS:   ALLOC (sizeof (EIO_STRUCT_STATVFS));
                          req->result = statvfs   (req->ptr1, (EIO_STRUCT_STATVFS *)req->ptr2); break;
      case EIO_FSTATVFS:  ALLOC (sizeof (EIO_STRUCT_STATVFS));
                          req->result = fstatvfs  (req->int1, (EIO_STRUCT_STATVFS *)req->ptr2); break;

      case EIO_CHOWN:     req->result = chown     (req->ptr1, req->int2, req->int3); break;
      case EIO_FCHOWN:    req->result = fchown    (req->int1, req->int2, req->int3); break;
      case EIO_CHMOD:     req->result = chmod     (req->ptr1, (eio_mode_t)req->int2); break;
      case EIO_FCHMOD:    req->result = fchmod    (req->int1, (eio_mode_t)req->int2); break;
      case EIO_TRUNCATE:  req->result = truncate  (req->ptr1, req->offs); break;
      case EIO_FTRUNCATE: req->result = ftruncate (req->int1, req->offs); break;

      case EIO_OPEN:      req->result = open      (req->ptr1, req->int1, (eio_mode_t)req->int2); break;
      case EIO_CLOSE:     req->result = close     (req->int1); break;
      case EIO_DUP2:      req->result = dup2      (req->int1, req->int2); break;
      case EIO_UNLINK:    req->result = unlink    (req->ptr1); break;
      case EIO_RMDIR:     req->result = rmdir     (req->ptr1); break;
      case EIO_MKDIR:     req->result = mkdir     (req->ptr1, (eio_mode_t)req->int2); break;
      case EIO_RENAME:    req->result = rename    (req->ptr1, req->ptr2); break;
      case EIO_LINK:      req->result = link      (req->ptr1, req->ptr2); break;
      case EIO_SYMLINK:   req->result = symlink   (req->ptr1, req->ptr2); break;
      case EIO_MKNOD:     req->result = mknod     (req->ptr1, (eio_mode_t)req->int2, (dev_t)req->offs); break;

      case EIO_REALPATH:  eio__realpath (req, self); break;

      case EIO_READLINK:  ALLOC (PATH_MAX);
                          req->result = readlink  (req->ptr1, req->ptr2, PATH_MAX); break;

      case EIO_SYNC:      req->result = 0; sync (); break;
      case EIO_FSYNC:     req->result = fsync     (req->int1); break;
      case EIO_FDATASYNC: req->result = fdatasync (req->int1); break;
      case EIO_SYNCFS:    req->result = eio__syncfs (req->int1); break;
      case EIO_SYNC_FILE_RANGE: req->result = eio__sync_file_range (req->int1, req->offs, req->size, req->int2); break;
      case EIO_MSYNC:     req->result = eio__msync (req->ptr2, req->size, req->int1); break;
      case EIO_MTOUCH:    req->result = eio__mtouch (req); break;
      case EIO_MLOCK:     req->result = eio__mlock (req->ptr2, req->size); break;
      case EIO_MLOCKALL:  req->result = eio__mlockall (req->int1); break;
      case EIO_FALLOCATE: req->result = eio__fallocate (req->int1, req->int2, req->offs, req->size); break;

      case EIO_READDIR:   eio__scandir (req, self); break;

      case EIO_BUSY:
#ifdef _WIN32
	Sleep (req->nv1 * 1e3);
#else
        {
          struct timeval tv;

          tv.tv_sec  = req->nv1;
          tv.tv_usec = (req->nv1 - tv.tv_sec) * 1e6;

          req->result = select (0, 0, 0, 0, &tv);
        }
#endif
        break;

      case EIO_UTIME:
      case EIO_FUTIME:
        {
          struct timeval tv[2];
          struct timeval *times;

          if (req->nv1 != -1. || req->nv2 != -1.)
            {
              tv[0].tv_sec  = req->nv1;
              tv[0].tv_usec = (req->nv1 - tv[0].tv_sec) * 1000000.;
              tv[1].tv_sec  = req->nv2;
              tv[1].tv_usec = (req->nv2 - tv[1].tv_sec) * 1000000.;

              times = tv;
            }
          else
            times = 0;

          req->result = req->type == EIO_FUTIME
                        ? futimes (req->int1, times)
                        : utimes  (req->ptr1, times);
        }
        break;

      case EIO_GROUP:
        abort (); /* handled in eio_request */

      case EIO_NOP:
        req->result = 0;
        break;

      case EIO_CUSTOM:
        req->feed (req);
        break;

      default:
        errno = ENOSYS;
        req->result = -1;
        break;
    }

  req->errorno = errno;
}

#ifndef EIO_NO_WRAPPERS

eio_req *eio_nop (int pri, eio_cb cb, void *data)
{
  REQ (EIO_NOP); SEND;
}

eio_req *eio_busy (double delay, int pri, eio_cb cb, void *data)
{
  REQ (EIO_BUSY); req->nv1 = delay; SEND;
}

eio_req *eio_sync (int pri, eio_cb cb, void *data)
{
  REQ (EIO_SYNC); SEND;
}

eio_req *eio_fsync (int fd, int pri, eio_cb cb, void *data)
{
  REQ (EIO_FSYNC); req->int1 = fd; SEND;
}

eio_req *eio_msync (void *addr, size_t length, int flags, int pri, eio_cb cb, void *data)
{
  REQ (EIO_MSYNC); req->ptr2 = addr; req->size = length; req->int1 = flags; SEND;
}

eio_req *eio_fdatasync (int fd, int pri, eio_cb cb, void *data)
{
  REQ (EIO_FDATASYNC); req->int1 = fd; SEND;
}

eio_req *eio_syncfs (int fd, int pri, eio_cb cb, void *data)
{
  REQ (EIO_SYNCFS); req->int1 = fd; SEND;
}

eio_req *eio_sync_file_range (int fd, off_t offset, size_t nbytes, unsigned int flags, int pri, eio_cb cb, void *data)
{
  REQ (EIO_SYNC_FILE_RANGE); req->int1 = fd; req->offs = offset; req->size = nbytes; req->int2 = flags; SEND;
}

eio_req *eio_mtouch (void *addr, size_t length, int flags, int pri, eio_cb cb, void *data)
{
  REQ (EIO_MTOUCH); req->ptr2 = addr; req->size = length; req->int1 = flags; SEND;
}

eio_req *eio_mlock (void *addr, size_t length, int pri, eio_cb cb, void *data)
{
  REQ (EIO_MLOCK); req->ptr2 = addr; req->size = length; SEND;
}

eio_req *eio_mlockall (int flags, int pri, eio_cb cb, void *data)
{
  REQ (EIO_MLOCKALL); req->int1 = flags; SEND;
}

eio_req *eio_fallocate (int fd, int mode, off_t offset, size_t len, int pri, eio_cb cb, void *data)
{
  REQ (EIO_FALLOCATE); req->int1 = fd; req->int2 = mode; req->offs = offset; req->size = len; SEND;
}

eio_req *eio_close (int fd, int pri, eio_cb cb, void *data)
{
  REQ (EIO_CLOSE); req->int1 = fd; SEND;
}

eio_req *eio_readahead (int fd, off_t offset, size_t length, int pri, eio_cb cb, void *data)
{
  REQ (EIO_READAHEAD); req->int1 = fd; req->offs = offset; req->size = length; SEND;
}

eio_req *eio_read (int fd, void *buf, size_t length, off_t offset, int pri, eio_cb cb, void *data)
{
  REQ (EIO_READ); req->int1 = fd; req->offs = offset; req->size = length; req->ptr2 = buf; SEND;
}

eio_req *eio_write (int fd, void *buf, size_t length, off_t offset, int pri, eio_cb cb, void *data)
{
  REQ (EIO_WRITE); req->int1 = fd; req->offs = offset; req->size = length; req->ptr2 = buf; SEND;
}

eio_req *eio_fstat (int fd, int pri, eio_cb cb, void *data)
{
  REQ (EIO_FSTAT); req->int1 = fd; SEND;
}

eio_req *eio_fstatvfs (int fd, int pri, eio_cb cb, void *data)
{
  REQ (EIO_FSTATVFS); req->int1 = fd; SEND;
}

eio_req *eio_futime (int fd, double atime, double mtime, int pri, eio_cb cb, void *data)
{
  REQ (EIO_FUTIME); req->int1 = fd; req->nv1 = atime; req->nv2 = mtime; SEND;
}

eio_req *eio_ftruncate (int fd, off_t offset, int pri, eio_cb cb, void *data)
{
  REQ (EIO_FTRUNCATE); req->int1 = fd; req->offs = offset; SEND;
}

eio_req *eio_fchmod (int fd, eio_mode_t mode, int pri, eio_cb cb, void *data)
{
  REQ (EIO_FCHMOD); req->int1 = fd; req->int2 = (long)mode; SEND;
}

eio_req *eio_fchown (int fd, eio_uid_t uid, eio_gid_t gid, int pri, eio_cb cb, void *data)
{
  REQ (EIO_FCHOWN); req->int1 = fd; req->int2 = (long)uid; req->int3 = (long)gid; SEND;
}

eio_req *eio_dup2 (int fd, int fd2, int pri, eio_cb cb, void *data)
{
  REQ (EIO_DUP2); req->int1 = fd; req->int2 = fd2; SEND;
}

eio_req *eio_sendfile (int out_fd, int in_fd, off_t in_offset, size_t length, int pri, eio_cb cb, void *data)
{
  REQ (EIO_SENDFILE); req->int1 = out_fd; req->int2 = in_fd; req->offs = in_offset; req->size = length; SEND;
}

eio_req *eio_open (const char *path, int flags, eio_mode_t mode, int pri, eio_cb cb, void *data)
{
  REQ (EIO_OPEN); PATH; req->int1 = flags; req->int2 = (long)mode; SEND;
}

eio_req *eio_utime (const char *path, double atime, double mtime, int pri, eio_cb cb, void *data)
{
  REQ (EIO_UTIME); PATH; req->nv1 = atime; req->nv2 = mtime; SEND;
}

eio_req *eio_truncate (const char *path, off_t offset, int pri, eio_cb cb, void *data)
{
  REQ (EIO_TRUNCATE); PATH; req->offs = offset; SEND;
}

eio_req *eio_chown (const char *path, eio_uid_t uid, eio_gid_t gid, int pri, eio_cb cb, void *data)
{
  REQ (EIO_CHOWN); PATH; req->int2 = (long)uid; req->int3 = (long)gid; SEND;
}

eio_req *eio_chmod (const char *path, eio_mode_t mode, int pri, eio_cb cb, void *data)
{
  REQ (EIO_CHMOD); PATH; req->int2 = (long)mode; SEND;
}

eio_req *eio_mkdir (const char *path, eio_mode_t mode, int pri, eio_cb cb, void *data)
{
  REQ (EIO_MKDIR); PATH; req->int2 = (long)mode; SEND;
}

static eio_req *
eio__1path (int type, const char *path, int pri, eio_cb cb, void *data)
{
  REQ (type); PATH; SEND;
}

eio_req *eio_readlink (const char *path, int pri, eio_cb cb, void *data)
{
  return eio__1path (EIO_READLINK, path, pri, cb, data);
}

eio_req *eio_realpath (const char *path, int pri, eio_cb cb, void *data)
{
  return eio__1path (EIO_REALPATH, path, pri, cb, data);
}

eio_req *eio_stat (const char *path, int pri, eio_cb cb, void *data)
{
  return eio__1path (EIO_STAT, path, pri, cb, data);
}

eio_req *eio_lstat (const char *path, int pri, eio_cb cb, void *data)
{
  return eio__1path (EIO_LSTAT, path, pri, cb, data);
}

eio_req *eio_statvfs (const char *path, int pri, eio_cb cb, void *data)
{
  return eio__1path (EIO_STATVFS, path, pri, cb, data);
}

eio_req *eio_unlink (const char *path, int pri, eio_cb cb, void *data)
{
  return eio__1path (EIO_UNLINK, path, pri, cb, data);
}

eio_req *eio_rmdir (const char *path, int pri, eio_cb cb, void *data)
{
  return eio__1path (EIO_RMDIR, path, pri, cb, data);
}

eio_req *eio_readdir (const char *path, int flags, int pri, eio_cb cb, void *data)
{
  REQ (EIO_READDIR); PATH; req->int1 = flags; SEND;
}

eio_req *eio_mknod (const char *path, eio_mode_t mode, dev_t dev, int pri, eio_cb cb, void *data)
{
  REQ (EIO_MKNOD); PATH; req->int2 = (long)mode; req->offs = (off_t)dev; SEND;
}

static eio_req *
eio__2path (int type, const char *path, const char *new_path, int pri, eio_cb cb, void *data)
{
  REQ (type); PATH;

  req->flags |= EIO_FLAG_PTR2_FREE;
  req->ptr2 = strdup (new_path);
  if (!req->ptr2)
    {
      eio_api_destroy (req);
      return 0;
    }

  SEND;
}

eio_req *eio_link (const char *path, const char *new_path, int pri, eio_cb cb, void *data)
{
  return eio__2path (EIO_LINK, path, new_path, pri, cb, data);
}

eio_req *eio_symlink (const char *path, const char *new_path, int pri, eio_cb cb, void *data)
{
  return eio__2path (EIO_SYMLINK, path, new_path, pri, cb, data);
}

eio_req *eio_rename (const char *path, const char *new_path, int pri, eio_cb cb, void *data)
{
  return eio__2path (EIO_RENAME, path, new_path, pri, cb, data);
}

eio_req *eio_custom (void (*execute)(eio_req *), int pri, eio_cb cb, void *data)
{
  REQ (EIO_CUSTOM); req->feed = execute; SEND;
}

#endif

eio_req *eio_grp (eio_cb cb, void *data)
{
  const int pri = EIO_PRI_MAX;

  REQ (EIO_GROUP); SEND;
}

#undef REQ
#undef PATH
#undef SEND

/*****************************************************************************/
/* grp functions */

void
eio_grp_feed (eio_req *grp, void (*feed)(eio_req *req), int limit)
{
  grp->int2 = limit;
  grp->feed = feed;

  grp_try_feed (grp);
}

void
eio_grp_limit (eio_req *grp, int limit)
{
  grp->int2 = limit;

  grp_try_feed (grp);
}

void
eio_grp_add (eio_req *grp, eio_req *req)
{
  assert (("cannot add requests to IO::AIO::GRP after the group finished", grp->int1 != 2));

  grp->flags |= EIO_FLAG_GROUPADD;

  ++grp->size;
  req->grp = grp;

  req->grp_prev = 0;
  req->grp_next = grp->grp_first;

  if (grp->grp_first)
    grp->grp_first->grp_prev = req;

  grp->grp_first = req;
}

/*****************************************************************************/
/* misc garbage */

eio_ssize_t
eio_sendfile_sync (int ofd, int ifd, off_t offset, size_t count)
{
  return eio__sendfile (ofd, ifd, offset, count);
}

