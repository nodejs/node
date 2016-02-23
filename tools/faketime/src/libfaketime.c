/*
 *  This file is part of libfaketime, version 0.9.6
 *
 *  libfaketime is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License v2 as published by the
 *  Free Software Foundation.
 *
 *  libfaketime is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License v2 along
 *  with the libfaketime; if not, write to the Free Software Foundation,
 *  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *      =======================================================================
 *      Global settings, includes, and macros                          === HEAD
 *      =======================================================================
 */

#define _GNU_SOURCE             /* required to get RTLD_NEXT defined */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <limits.h>

#include "time_ops.h"
#include "faketime_common.h"

/* pthread-handling contributed by David North, TDI in version 0.7 */
#ifdef PTHREAD
#include <pthread.h>
#endif

#include <sys/timeb.h>
#include <dlfcn.h>

#define BUFFERLEN   256

#ifndef __APPLE__
extern char *__progname;
#ifdef __sun
#include "sunos_endian.h"
#else
#include <endian.h>
#endif
#else
/* endianness related macros */
#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)

/* clock_gettime() and related clock definitions are missing on __APPLE__ */
#ifndef CLOCK_REALTIME
/* from GNU C Library time.h */
/* Identifier for system-wide realtime clock. ( == 1) */
#define CLOCK_REALTIME               CALENDAR_CLOCK
/* Monotonic system-wide clock. (== 0) */
#define CLOCK_MONOTONIC              SYSTEM_CLOCK
/* High-resolution timer from the CPU.  */
#define CLOCK_PROCESS_CPUTIME_ID     2
/* Thread-specific CPU-time clock.  */
#define CLOCK_THREAD_CPUTIME_ID      3
/* Monotonic system-wide clock, not adjusted for frequency scaling.  */
#define CLOCK_MONOTONIC_RAW          4
typedef int clockid_t;
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#endif

/* some systems lack raw clock */
#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW (CLOCK_MONOTONIC + 1)
#endif

/*
 * Per thread variable, which we turn on inside real_* calls to avoid modifying
 * time multiple times of for the whole process to prevent faking time
 */
static __thread bool dont_fake = false;

/* Wrapper for function calls, which we want to return system time */
#define DONT_FAKE_TIME(call)          \
  {                                   \
    bool dont_fake_orig = dont_fake;  \
    if (!dont_fake)                   \
    {                                 \
      dont_fake = true;               \
    }                                 \
    call;                             \
    dont_fake = dont_fake_orig;       \
  } while (0)

/* pointers to real (not faked) functions */
static int          (*real_stat)            (int, const char *, struct stat *);
static int          (*real_fstat)           (int, int, struct stat *);
static int          (*real_fstatat)         (int, int, const char *, struct stat *, int);
static int          (*real_lstat)           (int, const char *, struct stat *);
static int          (*real_stat64)          (int, const char *, struct stat64 *);
static int          (*real_fstat64)         (int, int , struct stat64 *);
static int          (*real_fstatat64)       (int, int , const char *, struct stat64 *, int);
static int          (*real_lstat64)         (int, const char *, struct stat64 *);
static time_t       (*real_time)            (time_t *);
static int          (*real_ftime)           (struct timeb *);
static int          (*real_gettimeofday)    (struct timeval *, void *);
static int          (*real_clock_gettime)   (clockid_t clk_id, struct timespec *tp);
#ifdef FAKE_INTERNAL_CALLS
static int          (*real___ftime)           (struct timeb *);
static int          (*real___gettimeofday)    (struct timeval *, void *);
static int          (*real___clock_gettime)   (clockid_t clk_id, struct timespec *tp);
#endif
#ifndef __APPLE__
#ifdef FAKE_TIMERS
static int          (*real_timer_settime_22)   (int timerid, int flags, const struct itimerspec *new_value,
                                                struct itimerspec * old_value);
static int          (*real_timer_settime_233)  (timer_t timerid, int flags,
                                                const struct itimerspec *new_value,
                                                struct itimerspec * old_value);
static int          (*real_timer_gettime_22)   (int timerid,
                                                struct itimerspec *curr_value);
static int          (*real_timer_gettime_233)  (timer_t timerid,
                                                struct itimerspec *curr_value);
#endif
#endif
#ifdef FAKE_SLEEP
static int          (*real_nanosleep)       (const struct timespec *req, struct timespec *rem);
static int          (*real_usleep)          (useconds_t usec);
static unsigned int (*real_sleep)           (unsigned int seconds);
static unsigned int (*real_alarm)           (unsigned int seconds);
static int          (*real_poll)            (struct pollfd *, nfds_t, int);
static int          (*real_ppoll)           (struct pollfd *, nfds_t, const struct timespec *, const sigset_t *);
static int          (*real_sem_timedwait)   (sem_t*, const struct timespec*);
#endif
#ifdef __APPLE__
static int          (*real_clock_get_time)  (clock_serv_t clock_serv, mach_timespec_t *cur_timeclockid_t);
static int          apple_clock_gettime     (clockid_t clk_id, struct timespec *tp);
static clock_serv_t clock_serv_real;
#endif

static int initialized = 0;

/* prototypes */
static int    fake_gettimeofday(struct timeval *tv);
static int    fake_clock_gettime(clockid_t clk_id, struct timespec *tp);

/** Semaphore protecting shared data */
static sem_t *shared_sem = NULL;

/** Data shared among faketime-spawned processes */
static struct ft_shared_s *ft_shared = NULL;

/** Storage format for timestamps written to file. Big endian.*/
struct saved_timestamp
{
  int64_t sec;
  uint64_t nsec;
};

static inline void timespec_from_saved (struct timespec *tp,
  struct saved_timestamp *saved)
{
  /* read as big endian */
  tp->tv_sec = be64toh(saved->sec);
  tp->tv_nsec = be64toh(saved->nsec);
}

/** Saved timestamps */
static struct saved_timestamp *stss = NULL;
static size_t infile_size;
static bool infile_set = false;

/** File fd to save timestamps to */
static int outfile = -1;

static bool limited_faking = false;
static long callcounter = 0;
static long ft_start_after_secs = -1;
static long ft_stop_after_secs = -1;
static long ft_start_after_ncalls = -1;
static long ft_stop_after_ncalls = -1;

static bool spawnsupport = false;
static int spawned = 0;
static char ft_spawn_target[1024];
static long ft_spawn_secs = -1;
static long ft_spawn_ncalls = -1;

static int fake_monotonic_clock = 1;
static int cache_enabled = 1;
static int cache_duration = 10;     /* cache fake time input for 10 seconds */

/*
 * Static timespec to store our startup time, followed by a load-time library
 * initialization declaration.
 */
static struct system_time_s ftpl_starttime = {{0, -1}, {0, -1}, {0, -1}};

static char user_faked_time_fmt[BUFSIZ] = {0};

/* User supplied base time to fake */
static struct timespec user_faked_time_timespec = {0, -1};
/* User supplied base time is set */
static bool user_faked_time_set = false;
static char user_faked_time_saved[BUFFERLEN] = {0};

/* Fractional user offset provided through FAKETIME env. var.*/
static struct timespec user_offset = {0, -1};
/* Speed up or slow down clock */
static double user_rate = 1.0;
static bool user_rate_set = false;
static struct timespec user_per_tick_inc = {0, -1};
static bool user_per_tick_inc_set = false;

enum ft_mode_t {FT_FREEZE, FT_START_AT, FT_NOOP} ft_mode = FT_FREEZE;

/* Time to fake is not provided through FAKETIME env. var. */
static bool parse_config_file = true;

void ft_cleanup (void) __attribute__ ((destructor));
void ftpl_init (void) __attribute__ ((constructor));


/*
 *      =======================================================================
 *      Shared memory related functions                                 === SHM
 *      =======================================================================
 */

static void ft_shm_init (void)
{
  int ticks_shm_fd;
  char sem_name[256], shm_name[256], *ft_shared_env = getenv("FAKETIME_SHARED");

  if (ft_shared_env != NULL)
  {
    if (sscanf(ft_shared_env, "%255s %255s", sem_name, shm_name) < 2)
    {
      printf("Error parsing semaphore name and shared memory id from string: %s", ft_shared_env);
      exit(1);
    }

    if (SEM_FAILED == (shared_sem = sem_open(sem_name, 0)))
    {
      perror("sem_open");
      exit(1);
    }

    if (-1 == (ticks_shm_fd = shm_open(shm_name, O_CREAT|O_RDWR, S_IWUSR|S_IRUSR)))
    {
      perror("shm_open");
      exit(1);
    }

    if (MAP_FAILED == (ft_shared = mmap(NULL, sizeof(struct ft_shared_s), PROT_READ|PROT_WRITE,
            MAP_SHARED, ticks_shm_fd, 0)))
    {
      perror("mmap");
      exit(1);
    }
  }
}

void ft_cleanup (void)
{
  /* detach from shared memory */
  if (ft_shared != NULL)
  {
    munmap(ft_shared, sizeof(uint64_t));
  }
  if (stss != NULL)
  {
    munmap(stss, infile_size);
  }
  if (shared_sem != NULL)
  {
    sem_close(shared_sem);
  }
}


/*
 *      =======================================================================
 *      Internal time retrieval                                     === INTTIME
 *      =======================================================================
 */

/* Get system time from system for all clocks */
static void system_time_from_system (struct system_time_s * systime)
{
#ifdef __APPLE__
  /* from http://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x */
  clock_serv_t cclock;
  mach_timespec_t mts;
  host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &clock_serv_real);
  (*real_clock_get_time)(clock_serv_real, &mts);
  systime->real.tv_sec = mts.tv_sec;
  systime->real.tv_nsec = mts.tv_nsec;
  host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
  (*real_clock_get_time)(cclock, &mts);
  mach_port_deallocate(mach_task_self(), cclock);
  systime->mon.tv_sec = mts.tv_sec;
  systime->mon.tv_nsec = mts.tv_nsec;
  systime->mon_raw.tv_sec = mts.tv_sec;
  systime->mon_raw.tv_nsec = mts.tv_nsec;
#else
  DONT_FAKE_TIME((*real_clock_gettime)(CLOCK_REALTIME, &systime->real));
  DONT_FAKE_TIME((*real_clock_gettime)(CLOCK_MONOTONIC, &systime->mon));
  DONT_FAKE_TIME((*real_clock_gettime)(CLOCK_MONOTONIC_RAW, &systime->mon_raw));
#endif
}

static void next_time(struct timespec *tp, struct timespec *ticklen)
{
  if (shared_sem != NULL)
  {
    struct timespec inc;
    /* lock */
    if (sem_wait(shared_sem) == -1)
    {
      perror("sem_wait");
      exit(1);
    }
    /* calculate and update elapsed time */
    timespecmul(ticklen, ft_shared->ticks, &inc);
    timespecadd(&user_faked_time_timespec, &inc, tp);
    (ft_shared->ticks)++;
    /* unlock */
    if (sem_post(shared_sem) == -1)
    {
      perror("sem_post");
      exit(1);
    }
  }
}


/*
 *      =======================================================================
 *      Saving & loading time                                          === SAVE
 *      =======================================================================
 */

static void save_time(struct timespec *tp)
{
  if ((shared_sem != NULL) && (outfile != -1))
  {
    struct saved_timestamp time_write;
    ssize_t written;
    size_t n = 0;

    time_write.sec = htobe64(tp->tv_sec);
    time_write.nsec = htobe64(tp->tv_nsec);

    /* lock */
    if (sem_wait(shared_sem) == -1)
    {
      perror("sem_wait");
      exit(1);
    }

    lseek(outfile, 0, SEEK_END);
    do
    {
      written = write(outfile, &(((char*)&time_write)[n]), sizeof(time_write) - n);
    }
    while (((written == -1) && (errno == EINTR)) ||
            (sizeof(time_write) < (n += written)));

    if ((written == -1) || (n < sizeof(time_write)))
    {
      perror("Saving timestamp to file failed");
    }

    /* unlock */
    if (sem_post(shared_sem) == -1)
    {
      perror("sem_post");
      exit(1);
    }
  }
}

/*
 * Provide faked time from file.
 * @return time is set from filen
 */
static bool load_time(struct timespec *tp)
{
  bool ret = false;
  if ((shared_sem != NULL) && (infile_set))
  {
    /* lock */
    if (sem_wait(shared_sem) == -1)
    {
      perror("sem_wait");
      exit(1);
    }

    if ((sizeof(stss[0]) * (ft_shared->file_idx + 1)) > infile_size)
    {
      /* we are out of timstamps to replay, return to faking time by rules
       * using last timestamp from file as the user provided timestamp */
      timespec_from_saved(&user_faked_time_timespec, &stss[(infile_size / sizeof(stss[0])) - 1 ]);

      if (ft_shared->ticks == 0)
      {
        /* we set shared memory to stop using infile */
        ft_shared->ticks = 1;
        system_time_from_system(&ftpl_starttime);
        ft_shared->start_time = ftpl_starttime;
      }
      else
      {
        ftpl_starttime = ft_shared->start_time;
      }

      munmap(stss, infile_size);
      infile_set = false;
    }
    else
    {
      timespec_from_saved(tp, &stss[ft_shared->file_idx]);
      ft_shared->file_idx++;
      ret = true;
    }

    /* unlock */
    if (sem_post(shared_sem) == -1)
    {
      perror("sem_post");
      exit(1);
    }
  }
  return ret;
}


/*
 *      =======================================================================
 *      Faked system functions: file related                     === FAKE(FILE)
 *      =======================================================================
 */

#ifdef FAKE_STAT

#ifndef NO_ATFILE
#ifndef _ATFILE_SOURCE
#define _ATFILE_SOURCE
#endif
#include <fcntl.h> /* Definition of AT_* constants */
#endif

#include <sys/stat.h>

static int fake_stat_disabled = 0;

#define FAKE_STRUCT_STAT_TIME(which) {                \
    struct timespec t = {buf->st_##which##time,       \
                         buf->st_##which##timensec};  \
    fake_clock_gettime(CLOCK_REALTIME, &t);           \
    buf->st_##which##time = t.tv_sec;                 \
    buf->st_##which##timensec = t.tv_nsec;            \
  } while (0)

static inline void fake_statbuf (struct stat *buf) {
#ifndef st_atime
  FAKE_STRUCT_STAT_TIME(c);
  FAKE_STRUCT_STAT_TIME(a);
  FAKE_STRUCT_STAT_TIME(m);
#else
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_ctim);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_atim);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_mtim);
#endif
}

static inline void fake_stat64buf (struct stat64 *buf) {
#ifndef st_atime
  FAKE_STRUCT_STAT_TIME(c);
  FAKE_STRUCT_STAT_TIME(a);
  FAKE_STRUCT_STAT_TIME(m);
#else
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_ctim);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_atim);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_mtim);
#endif
}

/* Contributed by Philipp Hachtmann in version 0.6 */
int __xstat (int ver, const char *path, struct stat *buf)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_stat)
  { /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original stat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_stat(ver, path, buf));
  if (result == -1)
  {
    return -1;
  }

   if (buf != NULL)
   {
     if (!fake_stat_disabled)
     {
       fake_statbuf(buf);
     }
   }

  return result;
}

/* Contributed by Philipp Hachtmann in version 0.6 */
int __fxstat (int ver, int fildes, struct stat *buf)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_fstat)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_fstat(ver, fildes, buf));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_statbuf(buf);
    }
  }
  return result;
}

/* Added in v0.8 as suggested by Daniel Kahn Gillmor */
#ifndef NO_ATFILE
int __fxstatat(int ver, int fildes, const char *filename, struct stat *buf, int flag)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_fstatat)
  { /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstatat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_fstatat(ver, fildes, filename, buf, flag));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_statbuf(buf);
    }
  }
  return result;
}
#endif

/* Contributed by Philipp Hachtmann in version 0.6 */
int __lxstat (int ver, const char *path, struct stat *buf)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_lstat)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original lstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_lstat(ver, path, buf));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_statbuf(buf);
    }
  }
  return result;
}

/* Contributed by Philipp Hachtmann in version 0.6 */
int __xstat64 (int ver, const char *path, struct stat64 *buf)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_stat64)
  { /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original stat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_stat64(ver, path, buf));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_stat64buf(buf);
    }
  }
  return result;
}

/* Contributed by Philipp Hachtmann in version 0.6 */
int __fxstat64 (int ver, int fildes, struct stat64 *buf)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_fstat64)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_fstat64(ver, fildes, buf));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_stat64buf(buf);
    }
  }
  return result;
}

/* Added in v0.8 as suggested by Daniel Kahn Gillmor */
#ifndef NO_ATFILE
int __fxstatat64 (int ver, int fildes, const char *filename, struct stat64 *buf, int flag)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_fstatat64)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstatat64() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_fstatat64(ver, fildes, filename, buf, flag));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_stat64buf(buf);
    }
  }
  return result;
}
#endif

/* Contributed by Philipp Hachtmann in version 0.6 */
int __lxstat64 (int ver, const char *path, struct stat64 *buf)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_lstat64)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original lstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_lstat64(ver, path, buf));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_stat64buf(buf);
    }
  }
  return result;
}
#endif

/*
 *      =======================================================================
 *      Faked system functions: sleep/alarm/poll/timer related  === FAKE(SLEEP)
 *      =======================================================================
 *      Contributed by Balint Reczey in v0.9.5
 */

#ifdef FAKE_SLEEP
/*
 * Faked nanosleep()
 */
int nanosleep(const struct timespec *req, struct timespec *rem)
{
  int result;
  struct timespec real_req;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_nanosleep == NULL)
  {
    return -1;
  }
  if (req != NULL)
  {
    if (user_rate_set && !dont_fake)
    {
      timespecmul(req, 1.0 / user_rate, &real_req);
    }
    else
    {
      real_req = *req;
    }
  }
  else
  {
    return -1;
  }

  DONT_FAKE_TIME(result = (*real_nanosleep)(&real_req, rem));
  if (result == -1)
  {
    return result;
  }

  /* fake returned parts */
  if ((rem != NULL) && ((rem->tv_sec != 0) || (rem->tv_nsec != 0)))
  {
    if (user_rate_set && !dont_fake)
    {
      timespecmul(rem, user_rate, rem);
    }
  }
  /* return the result to the caller */
  return result;
}

/*
 * Faked usleep()
 */
int usleep(useconds_t usec)
{
  int result;

  if (!initialized)
  {
    ftpl_init();
  }
  if (user_rate_set && !dont_fake)
  {
    struct timespec real_req;

    if (real_nanosleep == NULL)
    {
      /* fall back to usleep() */
      if (real_usleep == NULL)
      {
        return -1;
      }
      DONT_FAKE_TIME(result = (*real_usleep)((1.0 / user_rate) * usec));
      return result;
    }

    real_req.tv_sec = usec / 1000000;
    real_req.tv_nsec = (usec % 1000000) * 1000;
    timespecmul(&real_req, 1.0 / user_rate, &real_req);
    DONT_FAKE_TIME(result = (*real_nanosleep)(&real_req, NULL));
  }
  else
  {
    DONT_FAKE_TIME(result = (*real_usleep)(usec));
  }
  return result;
}

/*
 * Faked sleep()
 */
unsigned int sleep(unsigned int seconds)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (user_rate_set && !dont_fake)
  {
    if (real_nanosleep == NULL)
    {
      /* fall back to sleep */
      unsigned int ret;
      if (real_sleep == NULL)
      {
        return 0;
      }
      DONT_FAKE_TIME(ret = (*real_sleep)((1.0 / user_rate) * seconds));
      return (user_rate_set && !dont_fake)?(user_rate * ret):ret;
    }
    else
    {
      int result;
      struct timespec real_req = {seconds, 0}, rem;
      timespecmul(&real_req, 1.0 / user_rate, &real_req);
      DONT_FAKE_TIME(result = (*real_nanosleep)(&real_req, &rem));
      if (result == -1)
      {
        return 0;
      }

      /* fake returned parts */
      if ((rem.tv_sec != 0) || (rem.tv_nsec != 0))
      {
        timespecmul(&rem, user_rate, &rem);
      }
      /* return the result to the caller */
      return rem.tv_sec;
    }
  }
  else
  {
    /* no need to fake anything */
    unsigned int ret;
    DONT_FAKE_TIME(ret = (*real_sleep)(seconds));
    return ret;
  }
}

/*
 * Faked alarm()
 * @note due to rounding alarm(2) with faketime -f '+0 x7' won't wait 2/7
 * wall clock seconds but 0 seconds
 */
unsigned int alarm(unsigned int seconds)
{
  unsigned int ret;
  unsigned int seconds_real = (user_rate_set && !dont_fake)?((1.0 / user_rate) * seconds):seconds;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_alarm == NULL)
  {
    return -1;
  }

  DONT_FAKE_TIME(ret = (*real_alarm)(seconds_real));
  return (user_rate_set && !dont_fake)?(user_rate * ret):ret;
}

/*
 * Faked ppoll()
 */
int ppoll(struct pollfd *fds, nfds_t nfds,
    const struct timespec *timeout_ts, const sigset_t *sigmask)
{
  struct timespec real_timeout, *real_timeout_pt;
  int ret;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_ppoll == NULL)
  {
    return -1;
  }
  if (timeout_ts != NULL)
  {
    if (user_rate_set && !dont_fake && (timeout_ts->tv_sec > 0))
    {
      timespecmul(timeout_ts, 1.0 / user_rate, &real_timeout);
      real_timeout_pt = &real_timeout;
    }
    else
    {
      /* cast away constness */
      real_timeout_pt = (struct timespec *)timeout_ts;
    }
  }
  else
  {
    real_timeout_pt = NULL;
  }

  DONT_FAKE_TIME(ret = (*real_ppoll)(fds, nfds, real_timeout_pt, sigmask));
  return ret;
}

/*
 * Faked poll()
 */
int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
  int ret, timeout_real = (user_rate_set && !dont_fake && (timeout > 0))?(timeout / user_rate):timeout;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_poll == NULL)
  {
    return -1;
  }

  DONT_FAKE_TIME(ret = (*real_poll)(fds, nfds, timeout_real));
  return ret;
}

int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout)
{
  int result;
  struct timespec real_abs_timeout, *real_abs_timeout_pt;

  /* sanity check */
  if (abs_timeout == NULL)
  {
    return -1;
  }

  if (NULL == real_sem_timedwait)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original sem_timedwait() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  if (!dont_fake)
  {
    struct timespec tdiff, timeadj;

    timespecsub(abs_timeout, &ftpl_starttime.real, &tdiff);

    if (user_rate_set)
    {
      timespecmul(&tdiff, user_rate, &timeadj);
    }
    else
    {
        timeadj = tdiff;
    }
    timespecadd(&user_faked_time_timespec, &timeadj, &real_abs_timeout);
    real_abs_timeout_pt = &real_abs_timeout;
  }
  else
  {
    /* cast away constness */
    real_abs_timeout_pt = (struct timespec *)abs_timeout;
  }

  DONT_FAKE_TIME(result = (*real_sem_timedwait)(sem, real_abs_timeout_pt));
  return result;
}
#endif

#ifndef __APPLE__
#ifdef FAKE_TIMERS

/* timer related functions and structures */
typedef union {
  int int_member;
  timer_t timer_t_member;
} timer_t_or_int;

/*
 * Faketime's function implementation's compatibility mode
 */
typedef enum {FT_COMPAT_GLIBC_2_2, FT_COMPAT_GLIBC_2_3_3} ft_lib_compat;


/*
 * Faked timer_settime()
 * Does not affect timer speed when stepping clock with each time() call.
 */
static int
timer_settime_common(timer_t_or_int timerid, int flags,
         const struct itimerspec *new_value,
         struct itimerspec *old_value, ft_lib_compat compat)
{
  int result;
  struct itimerspec new_real;
  struct itimerspec *new_real_pt = &new_real;

  if (!initialized)
  {
    ftpl_init();
  }
  if (new_value == NULL)
  {
    new_real_pt = NULL;
  }
  else if (dont_fake)
  {
    /* cast away constness*/
    new_real_pt = (struct itimerspec *)new_value;
  }
  else
  {
    /* set it_value */
    if ((new_value->it_value.tv_sec != 0) ||
        (new_value->it_value.tv_nsec != 0))
    {
      if (flags & TIMER_ABSTIME)
      {
        struct timespec tdiff, timeadj;
        timespecsub(&new_value->it_value, &user_faked_time_timespec, &timeadj);
        if (user_rate_set)
        {
          timespecmul(&timeadj, 1.0/user_rate, &tdiff);
        }
        else
        {
          tdiff = timeadj;
        }
        /* only CLOCK_REALTIME is handled */
        timespecadd(&ftpl_starttime.real, &tdiff, &new_real.it_value);
      }
      else
      {
        if (user_rate_set)
        {
          timespecmul(&new_value->it_value, 1.0/user_rate, &new_real.it_value);
        }
        else
        {
          new_real.it_value = new_value->it_value;
        }
      }
    }
    else
    {
      new_real.it_value = new_value->it_value;
    }
    /* set it_interval */
    if (user_rate_set && ((new_value->it_interval.tv_sec != 0) ||
       (new_value->it_interval.tv_nsec != 0)))
    {
      timespecmul(&new_value->it_interval, 1.0/user_rate, &new_real.it_interval);
    }
    else
    {
      new_real.it_interval = new_value->it_interval;
    }
  }

  switch (compat)
  {
    case FT_COMPAT_GLIBC_2_2:
      DONT_FAKE_TIME(result = (*real_timer_settime_22)(timerid.int_member, flags,
                    new_real_pt, old_value));
      break;
    case FT_COMPAT_GLIBC_2_3_3:
       DONT_FAKE_TIME(result = (*real_timer_settime_233)(timerid.timer_t_member,
                    flags, new_real_pt, old_value));
       break;
    default:
      result = -1;
      break;
  }

  if (result == -1)
  {
    return result;
  }

  /* fake returned parts */
  if ((old_value != NULL) && !dont_fake)
  {
    if ((old_value->it_value.tv_sec != 0) ||
        (old_value->it_value.tv_nsec != 0))
    {
      result = fake_clock_gettime(CLOCK_REALTIME, &old_value->it_value);
    }
    if (user_rate_set && ((old_value->it_interval.tv_sec != 0) ||
       (old_value->it_interval.tv_nsec != 0)))
    {
      timespecmul(&old_value->it_interval, user_rate, &old_value->it_interval);
    }
  }

  /* return the result to the caller */
  return result;
}

/*
 * Faked timer_settime() compatible with implementation in GLIBC 2.2
 */
int timer_settime_22(int timerid, int flags,
         const struct itimerspec *new_value,
         struct itimerspec *old_value)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (real_timer_settime_22 == NULL)
  {
    return -1;
  }
  else
  {
    return (timer_settime_common((timer_t_or_int)timerid, flags, new_value, old_value,
            FT_COMPAT_GLIBC_2_2));
  }
}

/*
 * Faked timer_settime() compatible with implementation in GLIBC 2.3.3
 */
int timer_settime_233(timer_t timerid, int flags,
      const struct itimerspec *new_value,
      struct itimerspec *old_value)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (real_timer_settime_233 == NULL)
  {
    return -1;
  }
  else
  {
    return (timer_settime_common((timer_t_or_int)timerid, flags, new_value, old_value,
            FT_COMPAT_GLIBC_2_3_3));
  }
}

/*
 * Faked timer_gettime()
 * Does not affect timer speed when stepping clock with each time() call.
 */
int timer_gettime_common(timer_t_or_int timerid, struct itimerspec *curr_value, ft_lib_compat compat)
{
  int result;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_timer_gettime_233 == NULL)
  {
    return -1;
  }

  switch (compat)
  {
    case FT_COMPAT_GLIBC_2_2:
      DONT_FAKE_TIME(result = (*real_timer_gettime_22)(timerid.int_member, curr_value));
      break;
    case FT_COMPAT_GLIBC_2_3_3:
      DONT_FAKE_TIME(result = (*real_timer_gettime_233)(timerid.timer_t_member, curr_value));
      break;
    default:
      result = -1;
      break;
  }

  if (result == -1)
  {
    return result;
  }

  /* fake returned parts */
  if (curr_value != NULL)
  {
    if (user_rate_set && !dont_fake)
    {
      timespecmul(&curr_value->it_interval, user_rate, &curr_value->it_interval);
      timespecmul(&curr_value->it_value, user_rate, &curr_value->it_value);
    }
  }
  /* return the result to the caller */
  return result;
}

/*
 * Faked timer_gettime() compatible with implementation in GLIBC 2.2
 */
int timer_gettime_22(timer_t timerid, struct itimerspec *curr_value)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (real_timer_gettime_22 == NULL)
  {
    return -1;
  }
  else
  {
    return (timer_gettime_common((timer_t_or_int)timerid, curr_value,
         FT_COMPAT_GLIBC_2_2));
  }
}

/*
 * Faked timer_gettime() compatible with implementation in GLIBC 2.3.3
 */
int timer_gettime_233(timer_t timerid, struct itimerspec *curr_value)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (real_timer_gettime_233 == NULL)
  {
    return -1;
  }
  else
  {
    return (timer_gettime_common((timer_t_or_int)timerid, curr_value,
            FT_COMPAT_GLIBC_2_3_3));
  }
}

__asm__(".symver timer_gettime_22, timer_gettime@GLIBC_2.2");
__asm__(".symver timer_gettime_233, timer_gettime@@GLIBC_2.3.3");
__asm__(".symver timer_settime_22, timer_settime@GLIBC_2.2");
__asm__(".symver timer_settime_233, timer_settime@@GLIBC_2.3.3");

#endif
#endif


/*
 *      =======================================================================
 *      Faked system functions: basic time functions             === FAKE(TIME)
 *      =======================================================================
 */

/*
 * time() implementation using clock_gettime()
 * @note Does not check for EFAULT, see man 2 time
 */
time_t time(time_t *time_tptr)
{
  struct timespec tp;
  time_t result;

  if (!initialized)
  {
    ftpl_init();
  }
  DONT_FAKE_TIME(result = (*real_clock_gettime)(CLOCK_REALTIME, &tp));
  if (result == -1) return -1;

  /* pass the real current time to our faking version, overwriting it */
  (void)fake_clock_gettime(CLOCK_REALTIME, &tp);

  if (time_tptr != NULL)
  {
    *time_tptr = tp.tv_sec;
  }
  return tp.tv_sec;
}

int ftime(struct timeb *tb)
{
  struct timespec tp;
  int result;

  if (!initialized)
  {
    ftpl_init();
  }
  /* sanity check */
  if (tb == NULL)
    return 0;               /* ftime() always returns 0, see manpage */

  /* Check whether we've got a pointer to the real ftime() function yet */
  if (NULL == real_ftime)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original ftime() not found.\n");
#endif
    return 0; /* propagate error to caller */
  }

  /* initialize our TZ result with the real current time */
  DONT_FAKE_TIME(result = (*real_ftime)(tb));
  if (result == -1)
  {
    return result;
  }

  DONT_FAKE_TIME(result = (*real_clock_gettime)(CLOCK_REALTIME, &tp));
  if (result == -1) return -1;

  /* pass the real current time to our faking version, overwriting it */
  (void)fake_clock_gettime(CLOCK_REALTIME, &tp);

  tb->time = tp.tv_sec;
  tb->millitm = tp.tv_nsec / 1000000;

  /* return the result to the caller */
  return result; /* will always be 0 (see manpage) */
}

int gettimeofday(struct timeval *tv, void *tz)
{
  int result;

  if (!initialized)
  {
    ftpl_init();
  }
  /* sanity check */
  if (tv == NULL)
  {
    return -1;
  }

  /* Check whether we've got a pointer to the real ftime() function yet */
  if (NULL == real_gettimeofday)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original gettimeofday() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  /* initialize our result with the real current time */
  DONT_FAKE_TIME(result = (*real_gettimeofday)(tv, tz));
  if (result == -1) return result; /* original function failed */

  /* pass the real current time to our faking version, overwriting it */
  result = fake_gettimeofday(tv);

  /* return the result to the caller */
  return result;
}

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
  int result;

  if (!initialized)
  {
    ftpl_init();
  }
  /* sanity check */
  if (tp == NULL)
  {
    return -1;
  }

  if (NULL == real_clock_gettime)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original clock_gettime() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  /* initialize our result with the real current time */
  DONT_FAKE_TIME(result = (*real_clock_gettime)(clk_id, tp));
  if (result == -1) return result; /* original function failed */

  /* pass the real current time to our faking version, overwriting it */
  if (fake_monotonic_clock || clk_id != CLOCK_MONOTONIC)
  {
    result = fake_clock_gettime(clk_id, tp);
  }

  /* return the result to the caller */
  return result;
}


/*
 *      =======================================================================
 *      Parsing the user's faketime requests                          === PARSE
 *      =======================================================================
 */

static void parse_ft_string(const char *user_faked_time)
{
  struct tm user_faked_time_tm;
  char * tmp_time_fmt;

  if (!strncmp(user_faked_time, user_faked_time_saved, BUFFERLEN))
  {
      /* No change */
      return;
  }

  /* check whether the user gave us an absolute time to fake */
  switch (user_faked_time[0])
  {

    default:  /* Try and interpret this as a specified time */
      if (ft_mode != FT_NOOP) ft_mode = FT_FREEZE;
      user_faked_time_tm.tm_isdst = -1;
      if (NULL != strptime(user_faked_time, user_faked_time_fmt, &user_faked_time_tm))
      {
        user_faked_time_timespec.tv_sec = mktime(&user_faked_time_tm);
        user_faked_time_timespec.tv_nsec = 0;
        user_faked_time_set = true;
      }
      else
      {
        perror("Failed to parse FAKETIME timestamp");
        exit(EXIT_FAILURE);
      }
      break;

    case '+':
    case '-': /* User-specified offset */
      if (ft_mode != FT_NOOP) ft_mode = FT_START_AT;
      /* fractional time offsets contributed by Karl Chen in v0.8 */
      double frac_offset = atof(user_faked_time);

      /* offset is in seconds by default, but the string may contain
       * multipliers...
       */
      if (strchr(user_faked_time, 'm') != NULL) frac_offset *= 60;
      else if (strchr(user_faked_time, 'h') != NULL) frac_offset *= 60 * 60;
      else if (strchr(user_faked_time, 'd') != NULL) frac_offset *= 60 * 60 * 24;
      else if (strchr(user_faked_time, 'y') != NULL) frac_offset *= 60 * 60 * 24 * 365;

      user_offset.tv_sec = floor(frac_offset);
      user_offset.tv_nsec = (frac_offset - user_offset.tv_sec) * SEC_TO_nSEC;
      timespecadd(&ftpl_starttime.real, &user_offset, &user_faked_time_timespec);
      goto parse_modifiers;
      break;

      /* Contributed by David North, TDI in version 0.7 */
    case '@': /* Specific time, but clock along relative to that starttime */
      ft_mode = FT_START_AT;
      user_faked_time_tm.tm_isdst = -1;
      (void) strptime(&user_faked_time[1], user_faked_time_fmt, &user_faked_time_tm);

      user_faked_time_timespec.tv_sec = mktime(&user_faked_time_tm);
      user_faked_time_timespec.tv_nsec = 0;

      /* Reset starttime */
      system_time_from_system(&ftpl_starttime);
      goto parse_modifiers;
      break;

    case 'i':
    case 'x': /* Only modifiers are passed, don't fall back to strptime */
parse_modifiers:
      /* Speed-up / slow-down contributed by Karl Chen in v0.8 */
      if (strchr(user_faked_time, 'x') != NULL)
      {
        user_rate = atof(strchr(user_faked_time, 'x')+1);
        user_rate_set = true;
      }
      else if (NULL != (tmp_time_fmt = strchr(user_faked_time, 'i')))
      {
        double tick_inc = atof(tmp_time_fmt + 1);
        /* increment time with every time() call*/
        user_per_tick_inc.tv_sec = floor(tick_inc);
        user_per_tick_inc.tv_nsec = (tick_inc - user_per_tick_inc.tv_sec) * SEC_TO_nSEC ;
        user_per_tick_inc_set = true;
      }
      break;
  } // end of switch

  strncpy(user_faked_time_saved, user_faked_time, BUFFERLEN-1);
  user_faked_time_saved[BUFFERLEN-1] = 0;
#ifdef DEBUG
  fprintf(stderr, "new FAKETIME: %s\n", user_faked_time_saved);
#endif
}


/*
 *      =======================================================================
 *      Initialization                                                 === INIT
 *      =======================================================================
 */

void ftpl_init(void)
{
  char *tmp_env;

#ifdef __APPLE__
  const char *progname = getprogname();
#else
  const char *progname = __progname;
#endif

  /* Look up all real_* functions. NULL will mark missing ones. */
  real_stat =               dlsym(RTLD_NEXT, "__xstat");
  real_fstat =              dlsym(RTLD_NEXT, "__fxstat");
  real_fstatat =            dlsym(RTLD_NEXT, "__fxstatat");
  real_lstat =              dlsym(RTLD_NEXT, "__lxstat");
  real_stat64 =             dlsym(RTLD_NEXT,"__xstat64");
  real_fstat64 =            dlsym(RTLD_NEXT, "__fxstat64");
  real_fstatat64 =          dlsym(RTLD_NEXT, "__fxstatat64");
  real_lstat64 =            dlsym(RTLD_NEXT, "__lxstat64");
  real_time =               dlsym(RTLD_NEXT, "time");
  real_ftime =              dlsym(RTLD_NEXT, "ftime");
  real_gettimeofday =       dlsym(RTLD_NEXT, "gettimeofday");
#ifdef FAKE_SLEEP
  real_nanosleep =          dlsym(RTLD_NEXT, "nanosleep");
  real_usleep =             dlsym(RTLD_NEXT, "usleep");
  real_sleep =              dlsym(RTLD_NEXT, "sleep");
  real_alarm =              dlsym(RTLD_NEXT, "alarm");
  real_poll =               dlsym(RTLD_NEXT, "poll");
  real_ppoll =              dlsym(RTLD_NEXT, "ppoll");
  real_sem_timedwait =      dlsym(RTLD_NEXT, "sem_timedwait");
#endif
#ifdef FAKE_INTERNAL_CALLS
  real___ftime =              dlsym(RTLD_NEXT, "__ftime");
  real___gettimeofday =       dlsym(RTLD_NEXT, "__gettimeofday");
  real___clock_gettime  =     dlsym(RTLD_NEXT, "__clock_gettime");
#endif
#ifdef __APPLE__
  real_clock_get_time =     dlsym(RTLD_NEXT, "clock_get_time");
  real_clock_gettime  =     apple_clock_gettime;
#else
  real_clock_gettime  =     dlsym(RTLD_NEXT, "__clock_gettime");
  if (NULL == real_clock_gettime)
  {
    real_clock_gettime  =   dlsym(RTLD_NEXT, "clock_gettime");
  }
#ifdef FAKE_TIMERS
#if defined(__sun)
    real_timer_gettime_233 =  dlsym(RTLD_NEXT, "timer_gettime");
    real_timer_settime_233 =  dlsym(RTLD_NEXT, "timer_settime");
#else
  real_timer_settime_22 =   dlvsym(RTLD_NEXT, "timer_settime","GLIBC_2.2");
  real_timer_settime_233 =  dlvsym(RTLD_NEXT, "timer_settime","GLIBC_2.3.3");
  if (NULL == real_timer_settime_233)
  {
    real_timer_settime_233 =  dlsym(RTLD_NEXT, "timer_settime");
  }
  real_timer_gettime_22 =   dlvsym(RTLD_NEXT, "timer_gettime","GLIBC_2.2");
  real_timer_gettime_233 =  dlvsym(RTLD_NEXT, "timer_gettime","GLIBC_2.3.3");
  if (NULL == real_timer_gettime_233)
  {
    real_timer_gettime_233 =  dlsym(RTLD_NEXT, "timer_gettime");
  }
#endif
#endif
#endif

  initialized = 1;

  ft_shm_init();
#ifdef FAKE_STAT
  if (getenv("NO_FAKE_STAT")!=NULL)
  {
    fake_stat_disabled = 1;  //Note that this is NOT re-checked
  }
#endif

  if ((tmp_env = getenv("FAKETIME_CACHE_DURATION")) != NULL)
  {
    cache_duration = atoi(tmp_env);
  }
  if ((tmp_env = getenv("FAKETIME_NO_CACHE")) != NULL)
  {
    if (0 == strcmp(tmp_env, "1"))
    {
      cache_enabled = 0;
    }
  }
  if ((tmp_env = getenv("DONT_FAKE_MONOTONIC")) != NULL)
  {
    if (0 == strcmp(tmp_env, "1"))
    {
      fake_monotonic_clock = 0;
    }
  }
  /* Check whether we actually should be faking the returned timestamp. */

  /* We can prevent faking time for specified commands */
  if ((tmp_env = getenv("FAKETIME_SKIP_CMDS")) != NULL)
  {
    char *skip_cmd, *saveptr, *tmpvar;
    /* Don't mess with the env variable directly. */
    tmpvar = strdup(tmp_env);
    if (tmpvar != NULL)
    {
      skip_cmd = strtok_r(tmpvar, ",", &saveptr);
      while (skip_cmd != NULL)
      {
        if (0 == strcmp(progname, skip_cmd))
        {
          ft_mode = FT_NOOP;
          dont_fake = true;
          break;
        }
        skip_cmd = strtok_r(NULL, ",", &saveptr);
      }
      free(tmpvar);
      tmpvar = NULL;
    }
    else
    {
      fprintf(stderr, "Error: Could not copy the environment variable value.\n");
      exit(EXIT_FAILURE);
    }
  }

  /* We can limit faking time to specified commands */
  if ((tmp_env = getenv("FAKETIME_ONLY_CMDS")) != NULL)
  {
    char *only_cmd, *saveptr;
    bool cmd_matched = false;

    if (getenv("FAKETIME_SKIP_CMDS") != NULL)
    {
      fprintf(stderr, "Error: Both FAKETIME_SKIP_CMDS and FAKETIME_ONLY_CMDS can't be set.\n");
      exit(EXIT_FAILURE);
    }

    only_cmd = strtok_r(tmp_env, ",", &saveptr);
    while (only_cmd != NULL)
    {
      if (0 == strcmp(progname, only_cmd))
      {
        cmd_matched = true;
        break;
      }
      only_cmd = strtok_r(NULL, ",", &saveptr);
    }

    if (!cmd_matched)
    {
      ft_mode = FT_NOOP;
      dont_fake = true;
    }
  }

  if ((tmp_env = getenv("FAKETIME_START_AFTER_SECONDS")) != NULL)
  {
    ft_start_after_secs = atol(tmp_env);
    limited_faking = true;
  }
  if ((tmp_env = getenv("FAKETIME_STOP_AFTER_SECONDS")) != NULL)
  {
    ft_stop_after_secs = atol(tmp_env);
    limited_faking = true;
  }
  if ((tmp_env = getenv("FAKETIME_START_AFTER_NUMCALLS")) != NULL)
  {
    ft_start_after_ncalls = atol(tmp_env);
    limited_faking = true;
  }
  if ((tmp_env = getenv("FAKETIME_STOP_AFTER_NUMCALLS")) != NULL)
  {
    ft_stop_after_ncalls = atol(tmp_env);
    limited_faking = true;
  }

  /* check whether we should spawn an external command */
  if ((tmp_env = getenv("FAKETIME_SPAWN_TARGET")) != NULL)
  {
    spawnsupport = true;
    (void) strncpy(ft_spawn_target, getenv("FAKETIME_SPAWN_TARGET"), 1024);
    if ((tmp_env = getenv("FAKETIME_SPAWN_SECONDS")) != NULL)
    {
      ft_spawn_secs = atol(tmp_env);
    }
    if ((tmp_env = getenv("FAKETIME_SPAWN_NUMCALLS")) != NULL)
    {
      ft_spawn_ncalls = atol(tmp_env);
    }
  }

  if ((tmp_env = getenv("FAKETIME_SAVE_FILE")) != NULL)
  {
    if (-1 == (outfile = open(tmp_env, O_RDWR | O_APPEND | O_CLOEXEC | O_CREAT,
                              S_IWUSR | S_IRUSR)))
    {
      perror("Opening file for saving timestamps failed");
      exit(EXIT_FAILURE);
    }
  }

  /* load file only if reading timstamps from it is not finished yet */
  if ((tmp_env = getenv("FAKETIME_LOAD_FILE")) != NULL)
  {
    int infile = -1;
    struct stat sb;
    if (-1 == (infile = open(tmp_env, O_RDONLY|O_CLOEXEC)))
    {
      perror("Opening file for loading timestamps failed");
      exit(EXIT_FAILURE);
    }

    fstat(infile, &sb);
    if (sizeof(stss[0]) > (infile_size = sb.st_size))
    {
      printf("There are no timestamps in the provided file to load timestamps from");
      exit(EXIT_FAILURE);
    }

    if ((infile_size % sizeof(stss[0])) != 0)
    {
      printf("File size is not multiple of timestamp size. It is probably damaged.");
      exit(EXIT_FAILURE);
    }

    stss = mmap(NULL, infile_size, PROT_READ, MAP_SHARED, infile, 0);
    if (stss == MAP_FAILED)
    {
      perror("Mapping file for loading timestamps failed");
      exit(EXIT_FAILURE);
    }
    infile_set = true;
  }

  tmp_env = getenv("FAKETIME_FMT");
  if (tmp_env == NULL)
  {
    strcpy(user_faked_time_fmt, "%Y-%m-%d %T");
  }
  else
  {
    strncpy(user_faked_time_fmt, tmp_env, BUFSIZ);
  }

  if (shared_sem != 0)
  {
    if (sem_wait(shared_sem) == -1)
    {
      perror("sem_wait");
      exit(1);
    }
    if (ft_shared->start_time.real.tv_nsec == -1)
    {
      /* set up global start time */
      system_time_from_system(&ftpl_starttime);
      ft_shared->start_time = ftpl_starttime;
    }
    else
    {
      /** get preset start time */
      ftpl_starttime = ft_shared->start_time;
    }
    if (sem_post(shared_sem) == -1)
    {
      perror("sem_post");
      exit(1);
    }
  }
  else
  {
    system_time_from_system(&ftpl_starttime);
  }
  /* fake time supplied as environment variable? */
  if (NULL != (tmp_env = getenv("FAKETIME")))
  {
    parse_config_file = false;
    parse_ft_string(tmp_env);
  }
}


/*
 *      =======================================================================
 *      Helper functions                                             === HELPER
 *      =======================================================================
 */

static void remove_trailing_eols(char *line)
{
  char *endp = line + strlen(line);
  /*
   * erase the last char if it's a newline
   * or carriage return, and back up.
   * keep doing this, but don't back up
   * past the beginning of the string.
   */
# define is_eolchar(c) ((c) == '\n' || (c) == '\r')
  while (endp > line && is_eolchar(endp[-1]))
  {
    *--endp = '\0';
  }
}


/*
 *      =======================================================================
 *      Implementation of faked functions                        === FAKE(FAKE)
 *      =======================================================================
 */

int fake_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
  /* variables used for caching, introduced in version 0.6 */
  static time_t last_data_fetch = 0;  /* not fetched previously at first call */
  static int cache_expired = 1;       /* considered expired at first call */

  if (dont_fake) return 0;
  /* Per process timers are only sped up or slowed down */
  if ((clk_id == CLOCK_PROCESS_CPUTIME_ID ) || (clk_id == CLOCK_THREAD_CPUTIME_ID))
  {
    if (user_rate_set)
    {
      timespecmul(tp, user_rate, tp);
    }
    return 0;
  }

  /* Sanity check by Karl Chan since v0.8 */
  if (tp == NULL) return -1;

#ifdef PTHREAD_SINGLETHREADED_TIME
  static pthread_mutex_t time_mutex=PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&time_mutex);
  pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, (void *)&time_mutex);
#endif

  if ((limited_faking &&
     ((ft_start_after_ncalls != -1) || (ft_stop_after_ncalls != -1))) ||
     (spawnsupport && ft_spawn_ncalls))
  {
    if (callcounter < LONG_MAX) callcounter++;
  }

  if (limited_faking || spawnsupport)
  {
    struct timespec tmp_ts;
    /* For debugging, output #seconds and #calls */
    switch (clk_id)
    {
      case CLOCK_REALTIME:
#ifdef CLOCK_REALTIME_COARSE
      case CLOCK_REALTIME_COARSE:
#endif
        timespecsub(tp, &ftpl_starttime.real, &tmp_ts);
        break;
      case CLOCK_MONOTONIC:
#ifdef CLOCK_MONOTONIC_COARSE
      case CLOCK_MONOTONIC_COARSE:
#endif
        timespecsub(tp, &ftpl_starttime.mon, &tmp_ts);
        break;
      case CLOCK_MONOTONIC_RAW:
        timespecsub(tp, &ftpl_starttime.mon_raw, &tmp_ts);
        break;
      default:
        printf("Invalid clock_id for clock_gettime: %d", clk_id);
        exit(EXIT_FAILURE);
    }

    if (limited_faking)
    {
      /* Check whether we actually should be faking the returned timestamp. */
      /* fprintf(stderr, "(libfaketime limits -> runtime: %lu, callcounter: %lu\n", (*time_tptr - ftpl_starttime), callcounter); */
      if ((ft_start_after_secs != -1)   && (tmp_ts.tv_sec < ft_start_after_secs)) return 0;
      if ((ft_stop_after_secs != -1)    && (tmp_ts.tv_sec >= ft_stop_after_secs)) return 0;
      if ((ft_start_after_ncalls != -1) && (callcounter < ft_start_after_ncalls)) return 0;
      if ((ft_stop_after_ncalls != -1)  && (callcounter >= ft_stop_after_ncalls)) return 0;
      /* fprintf(stderr, "(libfaketime limits -> runtime: %lu, callcounter: %lu continues\n", (*time_tptr - ftpl_starttime), callcounter); */
    }

    if (spawnsupport)
    {
      /* check whether we should spawn an external command */
      if (spawned == 0)
      { /* exec external command once only */
        if (((tmp_ts.tv_sec == ft_spawn_secs) || (callcounter == ft_spawn_ncalls)) && (spawned == 0))
        {
          spawned = 1;
          (void) (system(ft_spawn_target) + 1);
        }
      }
    }
  }

  if (last_data_fetch > 0)
  {
    if ((tp->tv_sec - last_data_fetch) > cache_duration)
    {
      cache_expired = 1;
    }
    else
    {
      cache_expired = 0;
    }
  }

  if (cache_enabled == 0)
  {
    cache_expired = 1;
  }

  if (cache_expired == 1)
  {
    static char user_faked_time[BUFFERLEN]; /* changed to static for caching in v0.6 */
    /* initialize with default or env. variable */
    char *tmp_env;

    /* Can be enabled for testing ...
      fprintf(stderr, "***************++ Cache expired ++**************\n");
    */

    if (NULL != (tmp_env = getenv("FAKETIME")))
    {
      strncpy(user_faked_time, tmp_env, BUFFERLEN);
    }
    else
    {
      snprintf(user_faked_time, BUFFERLEN, "+0");
    }

    last_data_fetch = tp->tv_sec;
    /* fake time supplied as environment variable? */
    if (parse_config_file)
    {
      char custom_filename[BUFSIZ];
      char filename[BUFSIZ];
      FILE *faketimerc;
      /* check whether there's a .faketimerc in the user's home directory, or
       * a system-wide /etc/faketimerc present.
       * The /etc/faketimerc handling has been contributed by David Burley,
       * Jacob Moorman, and Wayne Davison of SourceForge, Inc. in version 0.6 */
      (void) snprintf(custom_filename, BUFSIZ, "%s", getenv("FAKETIME_TIMESTAMP_FILE"));
      (void) snprintf(filename, BUFSIZ, "%s/.faketimerc", getenv("HOME"));
      if ((faketimerc = fopen(custom_filename, "rt")) != NULL ||
          (faketimerc = fopen(filename, "rt")) != NULL ||
          (faketimerc = fopen("/etc/faketimerc", "rt")) != NULL)
      {
        char line[BUFFERLEN];
        while(fgets(line, BUFFERLEN, faketimerc) != NULL)
        {
          if ((strlen(line) > 1) && (line[0] != ' ') &&
              (line[0] != '#') && (line[0] != ';'))
          {
            remove_trailing_eols(line);
            strncpy(user_faked_time, line, BUFFERLEN-1);
            user_faked_time[BUFFERLEN-1] = 0;
            break;
          }
        }
        fclose(faketimerc);
      }
    } /* read fake time from file */
    parse_ft_string(user_faked_time);
  } /* cache had expired */

  if (infile_set)
  {
    if (load_time(tp))
    {
      return 0;
    }
  }

  /* check whether the user gave us an absolute time to fake */
  switch (ft_mode)
  {
    case FT_FREEZE:  /* a specified time */
      if (user_faked_time_set)
      {
        *tp = user_faked_time_timespec;
      }
      break;

    case FT_START_AT: /* User-specified offset */
      if (user_per_tick_inc_set)
      {
        /* increment time with every time() call*/
        next_time(tp, &user_per_tick_inc);
      }
      else
      {
        /* Speed-up / slow-down contributed by Karl Chen in v0.8 */
        struct timespec tdiff, timeadj;
        switch (clk_id)
        {
          case CLOCK_REALTIME:
#ifdef CLOCK_REALTIME_COARSE
          case CLOCK_REALTIME_COARSE:
#endif
            timespecsub(tp, &ftpl_starttime.real, &tdiff);
            break;
          case CLOCK_MONOTONIC:
#ifdef CLOCK_MONOTONIC_COARSE
          case CLOCK_MONOTONIC_COARSE:
#endif
            timespecsub(tp, &ftpl_starttime.mon, &tdiff);
            break;
          case CLOCK_MONOTONIC_RAW:
            timespecsub(tp, &ftpl_starttime.mon_raw, &tdiff);
            break;
          default:
            printf("Invalid clock_id for clock_gettime: %d", clk_id);
            exit(EXIT_FAILURE);
        } // end of switch (clk_id)
        if (user_rate_set)
        {
          timespecmul(&tdiff, user_rate, &timeadj);
        }
        else
        {
          timeadj = tdiff;
        }
        timespecadd(&user_faked_time_timespec, &timeadj, tp);
      }
      break;

    default:
      return -1;
  } // end of switch(ft_mode)

#ifdef PTHREAD_SINGLETHREADED_TIME
  pthread_cleanup_pop(1);
#endif
  save_time(tp);
  return 0;
}

int fake_gettimeofday(struct timeval *tv)
{
  struct timespec ts;
  int ret;
  ts.tv_sec = tv->tv_sec;
  ts.tv_nsec = tv->tv_usec * 1000  + ftpl_starttime.real.tv_nsec % 1000;

  ret = fake_clock_gettime(CLOCK_REALTIME, &ts);
  tv->tv_sec = ts.tv_sec;
  tv->tv_usec =ts.tv_nsec / 1000;

  return ret;
}


/*
 *      =======================================================================
 *      Faked system functions: Apple Mac OS X specific           === FAKE(OSX)
 *      =======================================================================
 */

#ifdef __APPLE__
/*
 * clock_gettime implementation for __APPLE__
 * @note It always behave like being called with CLOCK_REALTIME.
 */
static int apple_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
  int result;
  mach_timespec_t cur_timeclockid_t;
  (void) clk_id; /* unused */

  if (NULL == real_clock_get_time)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original clock_get_time() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  DONT_FAKE_TIME(result = (*real_clock_get_time)(clock_serv_real, &cur_timeclockid_t));
  tp->tv_sec =  cur_timeclockid_t.tv_sec;
  tp->tv_nsec = cur_timeclockid_t.tv_nsec;
  return result;
}

int clock_get_time(clock_serv_t clock_serv, mach_timespec_t *cur_timeclockid_t)
{
  int result;
  struct timespec ts;

  /*
   * Initialize our result with the real current time from CALENDAR_CLOCK.
   * This is a bit of cheating, but we don't keep track of obtained clock
   * services.
   */
  DONT_FAKE_TIME(result = (*real_clock_gettime)(CLOCK_REALTIME, &ts));
  if (result == -1) return result; /* original function failed */

  /* pass the real current time to our faking version, overwriting it */
  result = fake_clock_gettime(CLOCK_REALTIME, &ts);
  cur_timeclockid_t->tv_sec = ts.tv_sec;
  cur_timeclockid_t->tv_nsec = ts.tv_nsec;

  /* return the result to the caller */
  return result;
}
#endif


/*
 *      =======================================================================
 *      Faked system-internal functions                           === FAKE(INT)
 *      =======================================================================
 */

#ifdef FAKE_INTERNAL_CALLS
int __gettimeofday(struct timeval *tv, void *tz)
{
  int result;

  /* sanity check */
  if (tv == NULL)
  {
    return -1;
  }

  /* Check whether we've got a pointer to the real ftime() function yet */
  if (NULL == real___gettimeofday)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original __gettimeofday() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  /* initialize our result with the real current time */
  DONT_FAKE_TIME(result = (*real___gettimeofday)(tv, tz));
  if (result == -1) return result; /* original function failed */

  /* pass the real current time to our faking version, overwriting it */
  result = fake_gettimeofday(tv);

  /* return the result to the caller */
  return result;
}

int __clock_gettime(clockid_t clk_id, struct timespec *tp)
{
  int result;

  /* sanity check */
  if (tp == NULL)
  {
    return -1;
  }

  if (NULL == real___clock_gettime)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original __clock_gettime() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  /* initialize our result with the real current time */
  DONT_FAKE_TIME(result = (*real___clock_gettime)(clk_id, tp));
  if (result == -1) return result; /* original function failed */

  /* pass the real current time to our faking version, overwriting it */
  if (fake_monotonic_clock || clk_id != CLOCK_MONOTONIC)
  {
    result = fake_clock_gettime(clk_id, tp);
  }

  /* return the result to the caller */
  return result;
}

time_t __time(time_t *time_tptr)
{
  struct timespec tp;
  time_t result;

  DONT_FAKE_TIME(result = (*real_clock_gettime)(CLOCK_REALTIME, &tp));
  if (result == -1) return -1;

  /* pass the real current time to our faking version, overwriting it */
  (void)fake_clock_gettime(CLOCK_REALTIME, &tp);

  if (time_tptr != NULL)
  {
    *time_tptr = tp.tv_sec;
  }
  return tp.tv_sec;
}

int __ftime(struct timeb *tb)
{
  struct timespec tp;
  int result;

  /* sanity check */
  if (tb == NULL)
    return 0;               /* ftime() always returns 0, see manpage */

  /* Check whether we've got a pointer to the real ftime() function yet */
  if (NULL == real___ftime)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original ftime() not found.\n");
#endif
    return 0; /* propagate error to caller */
  }

  /* initialize our TZ result with the real current time */
  DONT_FAKE_TIME(result = (*real___ftime)(tb));
  if (result == -1)
  {
    return result;
  }

  DONT_FAKE_TIME(result = (*real_clock_gettime)(CLOCK_REALTIME, &tp));
  if (result == -1) return -1;

  /* pass the real current time to our faking version, overwriting it */
  (void)fake_clock_gettime(CLOCK_REALTIME, &tp);

  tb->time = tp.tv_sec;
  tb->millitm = tp.tv_nsec / 1000000;

  /* return the result to the caller */
  return result; /* will always be 0 (see manpage) */
}

#endif

/*
 * Editor modelines
 *
 * Local variables:
 * c-basic-offset: 2
 * tab-width: 2
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=2 tabstop=2 expandtab:
 * :indentSize=2:tabSize=2:noTabs=true:
 */

/* eof */
