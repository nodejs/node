#ifndef _WIN32
# include <errno.h>
# include <sys/time.h>
# include <sys/resource.h>
# include <time.h>
#endif /* _WIN32 */

#include "uv.h"
#include "clocks.h"
#include "wasi_types.h"
#include "uv_mapping.h"


#define UVWASI__WIN_TIME_AND_RETURN(handle, time)                             \
  do {                                                                        \
    FILETIME create;                                                          \
    FILETIME exit;                                                            \
    FILETIME system;                                                          \
    FILETIME user;                                                            \
    SYSTEMTIME sys_system;                                                    \
    SYSTEMTIME sys_user;                                                      \
    if (0 == GetProcessTimes((handle), &create, &exit, &system, &user)) {     \
      return uvwasi__translate_uv_error(                                      \
        uv_translate_sys_error(GetLastError())                                \
      );                                                                      \
    }                                                                         \
                                                                              \
    if (0 == FileTimeToSystemTime(&system, &sys_system)) {                    \
      return uvwasi__translate_uv_error(                                      \
        uv_translate_sys_error(GetLastError())                                \
      );                                                                      \
    }                                                                         \
                                                                              \
    if (0 == FileTimeToSystemTime(&user, &sys_user)) {                        \
      return uvwasi__translate_uv_error(                                      \
        uv_translate_sys_error(GetLastError())                                \
      );                                                                      \
    }                                                                         \
                                                                              \
    (time) = (((sys_system.wHour * 3600) + (sys_system.wMinute * 60) +        \
              sys_system.wSecond) * NANOS_PER_SEC) +                          \
             (sys_system.wMilliseconds * 1000000) +                           \
             (((sys_user.wHour * 3600) + (sys_user.wMinute * 60) +            \
              sys_user.wSecond) * NANOS_PER_SEC) +                            \
             (sys_user.wMilliseconds * 1000000);                              \
    return UVWASI_ESUCCESS;                                                   \
  } while (0)


#define UVWASI__CLOCK_GETTIME_AND_RETURN(clk, time)                           \
  do {                                                                        \
    struct timespec ts;                                                       \
    if (0 != clock_gettime((clk), &ts))                                       \
      return uvwasi__translate_uv_error(uv_translate_sys_error(errno));       \
    (time) = (ts.tv_sec * NANOS_PER_SEC) + ts.tv_nsec;                        \
    return UVWASI_ESUCCESS;                                                   \
  } while (0)


#define UVWASI__GETRUSAGE_AND_RETURN(who, time)                               \
  do {                                                                        \
    struct rusage ru;                                                         \
    if (0 != getrusage((who), &ru))                                           \
      return uvwasi__translate_uv_error(uv_translate_sys_error(errno));       \
    (time) = (ru.ru_utime.tv_sec * NANOS_PER_SEC) +                           \
             (ru.ru_utime.tv_usec * 1000) +                                   \
             (ru.ru_stime.tv_sec * NANOS_PER_SEC) +                           \
             (ru.ru_stime.tv_usec * 1000);                                    \
    return UVWASI_ESUCCESS;                                                   \
  } while (0)


#define UVWASI__OSX_THREADTIME_AND_RETURN(time)                               \
  do {                                                                        \
    mach_port_t thread;                                                       \
    thread_basic_info_data_t info;                                            \
    mach_msg_type_number_t count;                                             \
    count = THREAD_BASIC_INFO_COUNT;                                          \
    thread = pthread_mach_thread_np(pthread_self());                          \
    if (KERN_SUCCESS != thread_info(thread,                                   \
                                    THREAD_BASIC_INFO,                        \
                                    (thread_info_t) &info,                    \
                                    &count)) {                                \
      return UVWASI_ENOSYS;                                                   \
    }                                                                         \
    (time) = (info.user_time.seconds * NANOS_PER_SEC) +                       \
             (info.user_time.microseconds * 1000) +                           \
             (info.system_time.seconds * NANOS_PER_SEC) +                     \
             (info.system_time.microseconds * 1000);                          \
    return UVWASI_ESUCCESS;                                                   \
  } while (0)


#define UVWASI__WIN_GETRES_AND_RETURN(time)                                   \
  do {                                                                        \
    /* The GetProcessTimes() docs claim a resolution of 100 ns. */            \
    (time) = 100;                                                             \
    return UVWASI_ESUCCESS;                                                   \
  } while (0)


#define UVWASI__CLOCK_GETRES_AND_RETURN(clk, time)                            \
  do {                                                                        \
    struct timespec ts;                                                       \
    /* Try calling clock_getres(). If it doesn't succeed, then default to     \
       1000000. We implement all of the clocks, and some platforms (such as   \
       SmartOS) don't support all of the clocks, even though they define      \
       the constants for them. */                                             \
    if (0 != clock_getres((clk), &ts))                                        \
      (time) = 1000000;                                                       \
    else                                                                      \
      (time) = (ts.tv_sec * NANOS_PER_SEC) + ts.tv_nsec;                      \
    return UVWASI_ESUCCESS;                                                   \
  } while (0)


#define UVWASI__SLOW_GETRES_AND_RETURN(time)                                  \
  do {                                                                        \
    /* Assume a "worst case" of 1000000 ns resolution. */                     \
    (time) = 1000000;                                                         \
    return UVWASI_ESUCCESS;                                                   \
  } while (0)


uvwasi_errno_t uvwasi__clock_gettime_realtime(uvwasi_timestamp_t* time) {
  uv_timeval64_t tv;
  int r;

  r = uv_gettimeofday(&tv);
  if (r != 0)
    return uvwasi__translate_uv_error(r);

  *time = (tv.tv_sec * NANOS_PER_SEC) + (tv.tv_usec * 1000);
  return UVWASI_ESUCCESS;
}


uvwasi_errno_t uvwasi__clock_gettime_process_cputime(uvwasi_timestamp_t* time) {
#if defined(_WIN32)
  UVWASI__WIN_TIME_AND_RETURN(GetCurrentProcess(), *time);
#elif defined(CLOCK_PROCESS_CPUTIME_ID) && \
      !defined(__APPLE__)               && \
      !defined(__sun)
  UVWASI__CLOCK_GETTIME_AND_RETURN(CLOCK_PROCESS_CPUTIME_ID, *time);
#else
  UVWASI__GETRUSAGE_AND_RETURN(RUSAGE_SELF, *time);
#endif
}


uvwasi_errno_t uvwasi__clock_gettime_thread_cputime(uvwasi_timestamp_t* time) {
#if defined(_WIN32)
  UVWASI__WIN_TIME_AND_RETURN(GetCurrentThread(), *time);
#elif defined(__APPLE__)
  UVWASI__OSX_THREADTIME_AND_RETURN(*time);
#elif defined(CLOCK_THREAD_CPUTIME_ID) && !defined(__sun)
  UVWASI__CLOCK_GETTIME_AND_RETURN(CLOCK_THREAD_CPUTIME_ID, *time);
#else
# if defined(RUSAGE_LWP)
  UVWASI__GETRUSAGE_AND_RETURN(RUSAGE_LWP, *time);
# elif defined(RUSAGE_THREAD)
  UVWASI__GETRUSAGE_AND_RETURN(RUSAGE_THREAD, *time);
# else
  return UVWASI_ENOSYS;
# endif /* RUSAGE_LWP */
#endif
}


uvwasi_errno_t uvwasi__clock_getres_process_cputime(uvwasi_timestamp_t* time) {
#if defined(_WIN32)
  UVWASI__WIN_GETRES_AND_RETURN(*time);
#elif defined(CLOCK_PROCESS_CPUTIME_ID) && \
      !defined(__APPLE__)               && \
      !defined(__sun)
  UVWASI__CLOCK_GETRES_AND_RETURN(CLOCK_PROCESS_CPUTIME_ID, *time);
#else
  UVWASI__SLOW_GETRES_AND_RETURN(*time);
#endif
}


uvwasi_errno_t uvwasi__clock_getres_thread_cputime(uvwasi_timestamp_t* time) {
#if defined(_WIN32)
  UVWASI__WIN_GETRES_AND_RETURN(*time);
#elif defined(__APPLE__)
  UVWASI__SLOW_GETRES_AND_RETURN(*time);
#elif defined(CLOCK_THREAD_CPUTIME_ID) && !defined(__sun)
  UVWASI__CLOCK_GETTIME_AND_RETURN(CLOCK_THREAD_CPUTIME_ID, *time);
#elif defined(RUSAGE_THREAD) || defined(RUSAGE_LWP)
  UVWASI__SLOW_GETRES_AND_RETURN(*time);
#else
  return UVWASI_ENOSYS;
#endif
}
