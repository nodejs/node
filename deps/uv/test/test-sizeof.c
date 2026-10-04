/* Copyright libuv contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Check that all public uv.h struct sizes stay stable across changes.
 * Any deliberate ABI change requires updating the expected sizes here.
 * Add a new platform block when porting to a new OS or architecture.
 */

#include "task.h"
#include "uv.h"

#if defined(__ANDROID__)

# if defined(__x86_64__)
#  define SIZEOF_UV_LOOP_T             848
#  define SIZEOF_UV_REQ_T               64
#  define SIZEOF_UV_HANDLE_T            96
#  define SIZEOF_UV_STREAM_T           248
#  define SIZEOF_UV_TCP_T              248
#  define SIZEOF_UV_UDP_T              216
#  define SIZEOF_UV_PIPE_T             264
#  define SIZEOF_UV_TTY_T              288
#  define SIZEOF_UV_POLL_T             160
#  define SIZEOF_UV_TIMER_T            152
#  define SIZEOF_UV_PREPARE_T          120
#  define SIZEOF_UV_CHECK_T            120
#  define SIZEOF_UV_IDLE_T             120
#  define SIZEOF_UV_ASYNC_T            128
#  define SIZEOF_UV_PROCESS_T          136
#  define SIZEOF_UV_FS_EVENT_T         136
#  define SIZEOF_UV_FS_POLL_T          104
#  define SIZEOF_UV_SIGNAL_T           152
#  define SIZEOF_UV_SHUTDOWN_T          80
#  define SIZEOF_UV_WRITE_T            192
#  define SIZEOF_UV_CONNECT_T           96
#  define SIZEOF_UV_UDP_SEND_T         320
#  define SIZEOF_UV_FS_T               440
#  define SIZEOF_UV_WORK_T             128
#  define SIZEOF_UV_GETADDRINFO_T      160
#  define SIZEOF_UV_GETNAMEINFO_T     1320
#  define SIZEOF_UV_RANDOM_T           144
#  define SIZEOF_UV_LIB_T               16
#  define SIZEOF_UV_BUF_T               16
#  define SIZEOF_UV_DIR_T               56
#  define SIZEOF_UV_STAT_T             160
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T          16
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T           16
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           144
#  define SIZEOF_UV_METRICS_T          128
#  define SIZEOF_UV_DIRENT_T            16
#  define SIZEOF_UV_PASSWD_T            40
#  define SIZEOF_UV_GROUP_T             24
#  define SIZEOF_UV_CPU_INFO_T          56
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 80
#  define SIZEOF_UV_ENV_ITEM_T          16
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_T             8
#  define SIZEOF_UV_MUTEX_T             40
#  define SIZEOF_UV_RWLOCK_T            56
#  define SIZEOF_UV_SEM_T               16
#  define SIZEOF_UV_COND_T              48
#  define SIZEOF_UV_ONCE_T               4
#  define SIZEOF_UV_KEY_T                4
#  define SIZEOF_UV_BARRIER_T           32
#  define SIZEOF_UV_THREAD_OPTIONS_T    16

# elif defined(__aarch64__)
#  define SIZEOF_UV_LOOP_T             848
#  define SIZEOF_UV_REQ_T               64
#  define SIZEOF_UV_HANDLE_T            96
#  define SIZEOF_UV_STREAM_T           248
#  define SIZEOF_UV_TCP_T              248
#  define SIZEOF_UV_UDP_T              216
#  define SIZEOF_UV_PIPE_T             264
#  define SIZEOF_UV_TTY_T              288
#  define SIZEOF_UV_POLL_T             160
#  define SIZEOF_UV_TIMER_T            152
#  define SIZEOF_UV_PREPARE_T          120
#  define SIZEOF_UV_CHECK_T            120
#  define SIZEOF_UV_IDLE_T             120
#  define SIZEOF_UV_ASYNC_T            128
#  define SIZEOF_UV_PROCESS_T          136
#  define SIZEOF_UV_FS_EVENT_T         136
#  define SIZEOF_UV_FS_POLL_T          104
#  define SIZEOF_UV_SIGNAL_T           152
#  define SIZEOF_UV_SHUTDOWN_T          80
#  define SIZEOF_UV_WRITE_T            192
#  define SIZEOF_UV_CONNECT_T           96
#  define SIZEOF_UV_UDP_SEND_T         320
#  define SIZEOF_UV_FS_T               440
#  define SIZEOF_UV_WORK_T             128
#  define SIZEOF_UV_GETADDRINFO_T      160
#  define SIZEOF_UV_GETNAMEINFO_T     1320
#  define SIZEOF_UV_RANDOM_T           144
#  define SIZEOF_UV_LIB_T               16
#  define SIZEOF_UV_BUF_T               16
#  define SIZEOF_UV_DIR_T               56
#  define SIZEOF_UV_STAT_T             160
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T          16
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T           16
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           144
#  define SIZEOF_UV_METRICS_T          128
#  define SIZEOF_UV_DIRENT_T            16
#  define SIZEOF_UV_PASSWD_T            40
#  define SIZEOF_UV_GROUP_T             24
#  define SIZEOF_UV_CPU_INFO_T          56
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 80
#  define SIZEOF_UV_ENV_ITEM_T          16
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_T             8
#  define SIZEOF_UV_MUTEX_T             40
#  define SIZEOF_UV_RWLOCK_T            56
#  define SIZEOF_UV_SEM_T               16
#  define SIZEOF_UV_COND_T              48
#  define SIZEOF_UV_ONCE_T               4
#  define SIZEOF_UV_KEY_T                4
#  define SIZEOF_UV_BARRIER_T           32
#  define SIZEOF_UV_THREAD_OPTIONS_T    16

# elif defined(__arm__)
#  define SIZEOF_UV_LOOP_T             456
#  define SIZEOF_UV_REQ_T               32
#  define SIZEOF_UV_HANDLE_T            48
#  define SIZEOF_UV_STREAM_T           132
#  define SIZEOF_UV_TCP_T              132
#  define SIZEOF_UV_UDP_T              112
#  define SIZEOF_UV_PIPE_T             140
#  define SIZEOF_UV_TTY_T              172
#  define SIZEOF_UV_POLL_T              84
#  define SIZEOF_UV_TIMER_T             88
#  define SIZEOF_UV_PREPARE_T           60
#  define SIZEOF_UV_CHECK_T             60
#  define SIZEOF_UV_IDLE_T              60
#  define SIZEOF_UV_ASYNC_T             64
#  define SIZEOF_UV_PROCESS_T           68
#  define SIZEOF_UV_FS_EVENT_T          68
#  define SIZEOF_UV_FS_POLL_T           52
#  define SIZEOF_UV_SIGNAL_T            80
#  define SIZEOF_UV_SHUTDOWN_T          40
#  define SIZEOF_UV_WRITE_T            100
#  define SIZEOF_UV_CONNECT_T           48
#  define SIZEOF_UV_UDP_SEND_T         224
#  define SIZEOF_UV_FS_T               296
#  define SIZEOF_UV_WORK_T              64
#  define SIZEOF_UV_GETADDRINFO_T       80
#  define SIZEOF_UV_GETNAMEINFO_T     1256
#  define SIZEOF_UV_RANDOM_T            72
#  define SIZEOF_UV_LIB_T                8
#  define SIZEOF_UV_BUF_T                8
#  define SIZEOF_UV_DIR_T               28
#  define SIZEOF_UV_STAT_T             128
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T           8
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T            8
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           128
#  define SIZEOF_UV_METRICS_T           80
#  define SIZEOF_UV_DIRENT_T             8
#  define SIZEOF_UV_PASSWD_T            20
#  define SIZEOF_UV_GROUP_T             12
#  define SIZEOF_UV_CPU_INFO_T          48
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 72
#  define SIZEOF_UV_ENV_ITEM_T           8
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_T             4
#  define SIZEOF_UV_MUTEX_T              4
#  define SIZEOF_UV_RWLOCK_T            40
#  define SIZEOF_UV_SEM_T                4
#  define SIZEOF_UV_COND_T               4
#  define SIZEOF_UV_ONCE_T               4
#  define SIZEOF_UV_KEY_T                4
#  define SIZEOF_UV_BARRIER_T           32
#  define SIZEOF_UV_THREAD_OPTIONS_T     8

# elif defined(__i386__)
#  define SIZEOF_UV_LOOP_T             452
#  define SIZEOF_UV_REQ_T               32
#  define SIZEOF_UV_HANDLE_T            48
#  define SIZEOF_UV_STREAM_T           132
#  define SIZEOF_UV_TCP_T              132
#  define SIZEOF_UV_UDP_T              112
#  define SIZEOF_UV_PIPE_T             140
#  define SIZEOF_UV_TTY_T              172
#  define SIZEOF_UV_POLL_T              84
#  define SIZEOF_UV_TIMER_T             88
#  define SIZEOF_UV_PREPARE_T           60
#  define SIZEOF_UV_CHECK_T             60
#  define SIZEOF_UV_IDLE_T              60
#  define SIZEOF_UV_ASYNC_T             64
#  define SIZEOF_UV_PROCESS_T           68
#  define SIZEOF_UV_FS_EVENT_T          68
#  define SIZEOF_UV_FS_POLL_T           52
#  define SIZEOF_UV_SIGNAL_T            80
#  define SIZEOF_UV_SHUTDOWN_T          40
#  define SIZEOF_UV_WRITE_T            100
#  define SIZEOF_UV_CONNECT_T           48
#  define SIZEOF_UV_UDP_SEND_T         224
#  define SIZEOF_UV_FS_T               288
#  define SIZEOF_UV_WORK_T              64
#  define SIZEOF_UV_GETADDRINFO_T       80
#  define SIZEOF_UV_GETNAMEINFO_T     1256
#  define SIZEOF_UV_RANDOM_T            72
#  define SIZEOF_UV_LIB_T                8
#  define SIZEOF_UV_BUF_T                8
#  define SIZEOF_UV_DIR_T               28
#  define SIZEOF_UV_STAT_T             128
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T           8
#  define SIZEOF_UV_TIMESPEC64_T        12
#  define SIZEOF_UV_TIMEVAL_T            8
#  define SIZEOF_UV_TIMEVAL64_T         12
#  define SIZEOF_UV_RUSAGE_T           128
#  define SIZEOF_UV_METRICS_T           76
#  define SIZEOF_UV_DIRENT_T             8
#  define SIZEOF_UV_PASSWD_T            20
#  define SIZEOF_UV_GROUP_T             12
#  define SIZEOF_UV_CPU_INFO_T          48
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 72
#  define SIZEOF_UV_ENV_ITEM_T           8
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_T             4
#  define SIZEOF_UV_MUTEX_T              4
#  define SIZEOF_UV_RWLOCK_T            40
#  define SIZEOF_UV_SEM_T                4
#  define SIZEOF_UV_COND_T               4
#  define SIZEOF_UV_ONCE_T               4
#  define SIZEOF_UV_KEY_T                4
#  define SIZEOF_UV_BARRIER_T           32
#  define SIZEOF_UV_THREAD_OPTIONS_T     8

# else
#  define UV_SIZEOF_SKIP "Android: sizes not recorded for this arch/pointer-size"
# endif /* arch */

#elif defined(__linux__)

# if defined(__x86_64__)
#  define SIZEOF_UV_LOOP_T             848
#  define SIZEOF_UV_REQ_T               64
#  define SIZEOF_UV_HANDLE_T            96
#  define SIZEOF_UV_STREAM_T           248
#  define SIZEOF_UV_TCP_T              248
#  define SIZEOF_UV_UDP_T              216
#  define SIZEOF_UV_PIPE_T             264
#  define SIZEOF_UV_TTY_T              312
#  define SIZEOF_UV_POLL_T             160
#  define SIZEOF_UV_TIMER_T            152
#  define SIZEOF_UV_PREPARE_T          120
#  define SIZEOF_UV_CHECK_T            120
#  define SIZEOF_UV_IDLE_T             120
#  define SIZEOF_UV_ASYNC_T            128
#  define SIZEOF_UV_PROCESS_T          136
#  define SIZEOF_UV_FS_EVENT_T         136
#  define SIZEOF_UV_FS_POLL_T          104
#  define SIZEOF_UV_SIGNAL_T           152
#  define SIZEOF_UV_SHUTDOWN_T          80
#  define SIZEOF_UV_WRITE_T            192
#  define SIZEOF_UV_CONNECT_T           96
#  define SIZEOF_UV_UDP_SEND_T         320
#  define SIZEOF_UV_FS_T               440
#  define SIZEOF_UV_WORK_T             128
#  define SIZEOF_UV_GETADDRINFO_T      160
#  define SIZEOF_UV_GETNAMEINFO_T     1320
#  define SIZEOF_UV_RANDOM_T           144
#  define SIZEOF_UV_LIB_T               16
#  define SIZEOF_UV_BUF_T               16
#  define SIZEOF_UV_DIR_T               56
#  define SIZEOF_UV_STAT_T             160
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T          16
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T           16
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           144
#  define SIZEOF_UV_METRICS_T          128
#  define SIZEOF_UV_DIRENT_T            16
#  define SIZEOF_UV_PASSWD_T            40
#  define SIZEOF_UV_GROUP_T             24
#  define SIZEOF_UV_CPU_INFO_T          56
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 80
#  define SIZEOF_UV_ENV_ITEM_T          16
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_T             8
#  define SIZEOF_UV_MUTEX_T             40
#  define SIZEOF_UV_RWLOCK_T            56
#  define SIZEOF_UV_SEM_T               32
#  define SIZEOF_UV_COND_T              48
#  define SIZEOF_UV_ONCE_T               4
#  define SIZEOF_UV_KEY_T                4
#  define SIZEOF_UV_BARRIER_T           32
#  define SIZEOF_UV_THREAD_OPTIONS_T    16

# elif defined(__i386__)
#  define SIZEOF_UV_LOOP_T             464
#  define SIZEOF_UV_REQ_T               32
#  define SIZEOF_UV_HANDLE_T            48
#  define SIZEOF_UV_STREAM_T           132
#  define SIZEOF_UV_TCP_T              132
#  define SIZEOF_UV_UDP_T              112
#  define SIZEOF_UV_PIPE_T             140
#  define SIZEOF_UV_TTY_T              196
#  define SIZEOF_UV_POLL_T              84
#  define SIZEOF_UV_TIMER_T             88
#  define SIZEOF_UV_PREPARE_T           60
#  define SIZEOF_UV_CHECK_T             60
#  define SIZEOF_UV_IDLE_T              60
#  define SIZEOF_UV_ASYNC_T             64
#  define SIZEOF_UV_PROCESS_T           68
#  define SIZEOF_UV_FS_EVENT_T          68
#  define SIZEOF_UV_FS_POLL_T           52
#  define SIZEOF_UV_SIGNAL_T            80
#  define SIZEOF_UV_SHUTDOWN_T          40
#  define SIZEOF_UV_WRITE_T            100
#  define SIZEOF_UV_CONNECT_T           48
#  define SIZEOF_UV_UDP_SEND_T         224
#  define SIZEOF_UV_FS_T               292
#  define SIZEOF_UV_WORK_T              64
#  define SIZEOF_UV_GETADDRINFO_T       80
#  define SIZEOF_UV_GETNAMEINFO_T     1256
#  define SIZEOF_UV_RANDOM_T            72
#  define SIZEOF_UV_LIB_T                8
#  define SIZEOF_UV_BUF_T                8
#  define SIZEOF_UV_DIR_T               28
#  define SIZEOF_UV_STAT_T             128
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T           8
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T            8
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           128
#  define SIZEOF_UV_METRICS_T           76
#  define SIZEOF_UV_DIRENT_T             8
#  define SIZEOF_UV_PASSWD_T            20
#  define SIZEOF_UV_GROUP_T             12
#  define SIZEOF_UV_CPU_INFO_T          48
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 72
#  define SIZEOF_UV_ENV_ITEM_T           8
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_T             4
#  define SIZEOF_UV_MUTEX_T             24
#  define SIZEOF_UV_RWLOCK_T            32
#  define SIZEOF_UV_SEM_T               16
#  define SIZEOF_UV_COND_T              48
#  define SIZEOF_UV_ONCE_T               4
#  define SIZEOF_UV_KEY_T                4
#  define SIZEOF_UV_BARRIER_T           20
#  define SIZEOF_UV_THREAD_OPTIONS_T     8

# elif defined(__aarch64__)
#  define SIZEOF_UV_LOOP_T             856
#  define SIZEOF_UV_REQ_T               64
#  define SIZEOF_UV_HANDLE_T            96
#  define SIZEOF_UV_STREAM_T           248
#  define SIZEOF_UV_TCP_T              248
#  define SIZEOF_UV_UDP_T              216
#  define SIZEOF_UV_PIPE_T             264
#  define SIZEOF_UV_TTY_T              312
#  define SIZEOF_UV_POLL_T             160
#  define SIZEOF_UV_TIMER_T            152
#  define SIZEOF_UV_PREPARE_T          120
#  define SIZEOF_UV_CHECK_T            120
#  define SIZEOF_UV_IDLE_T             120
#  define SIZEOF_UV_ASYNC_T            128
#  define SIZEOF_UV_PROCESS_T          136
#  define SIZEOF_UV_FS_EVENT_T         136
#  define SIZEOF_UV_FS_POLL_T          104
#  define SIZEOF_UV_SIGNAL_T           152
#  define SIZEOF_UV_SHUTDOWN_T          80
#  define SIZEOF_UV_WRITE_T            192
#  define SIZEOF_UV_CONNECT_T           96
#  define SIZEOF_UV_UDP_SEND_T         320
#  define SIZEOF_UV_FS_T               440
#  define SIZEOF_UV_WORK_T             128
#  define SIZEOF_UV_GETADDRINFO_T      160
#  define SIZEOF_UV_GETNAMEINFO_T     1320
#  define SIZEOF_UV_RANDOM_T           144
#  define SIZEOF_UV_LIB_T               16
#  define SIZEOF_UV_BUF_T               16
#  define SIZEOF_UV_DIR_T               56
#  define SIZEOF_UV_STAT_T             160
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T          16
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T           16
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           144
#  define SIZEOF_UV_METRICS_T          128
#  define SIZEOF_UV_DIRENT_T            16
#  define SIZEOF_UV_PASSWD_T            40
#  define SIZEOF_UV_GROUP_T             24
#  define SIZEOF_UV_CPU_INFO_T          56
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 80
#  define SIZEOF_UV_ENV_ITEM_T          16
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_T             8
#  define SIZEOF_UV_MUTEX_T             48
#  define SIZEOF_UV_RWLOCK_T            56
#  define SIZEOF_UV_SEM_T               32
#  define SIZEOF_UV_COND_T              48
#  define SIZEOF_UV_ONCE_T               4
#  define SIZEOF_UV_KEY_T                4
#  define SIZEOF_UV_BARRIER_T           32
#  define SIZEOF_UV_THREAD_OPTIONS_T    16

# elif defined(__riscv) && __riscv_xlen == 64
#  define SIZEOF_UV_LOOP_T             848
#  define SIZEOF_UV_REQ_T               64
#  define SIZEOF_UV_HANDLE_T            96
#  define SIZEOF_UV_STREAM_T           248
#  define SIZEOF_UV_TCP_T              248
#  define SIZEOF_UV_UDP_T              216
#  define SIZEOF_UV_PIPE_T             264
#  define SIZEOF_UV_TTY_T              312
#  define SIZEOF_UV_POLL_T             160
#  define SIZEOF_UV_TIMER_T            152
#  define SIZEOF_UV_PREPARE_T          120
#  define SIZEOF_UV_CHECK_T            120
#  define SIZEOF_UV_IDLE_T             120
#  define SIZEOF_UV_ASYNC_T            128
#  define SIZEOF_UV_PROCESS_T          136
#  define SIZEOF_UV_FS_EVENT_T         136
#  define SIZEOF_UV_FS_POLL_T          104
#  define SIZEOF_UV_SIGNAL_T           152
#  define SIZEOF_UV_SHUTDOWN_T          80
#  define SIZEOF_UV_WRITE_T            192
#  define SIZEOF_UV_CONNECT_T           96
#  define SIZEOF_UV_UDP_SEND_T         320
#  define SIZEOF_UV_FS_T               440
#  define SIZEOF_UV_WORK_T             128
#  define SIZEOF_UV_GETADDRINFO_T      160
#  define SIZEOF_UV_GETNAMEINFO_T     1320
#  define SIZEOF_UV_RANDOM_T           144
#  define SIZEOF_UV_LIB_T               16
#  define SIZEOF_UV_BUF_T               16
#  define SIZEOF_UV_DIR_T               56
#  define SIZEOF_UV_STAT_T             160
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T          16
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T           16
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           144
#  define SIZEOF_UV_METRICS_T          128
#  define SIZEOF_UV_DIRENT_T            16
#  define SIZEOF_UV_PASSWD_T            40
#  define SIZEOF_UV_GROUP_T             24
#  define SIZEOF_UV_CPU_INFO_T          56
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 80
#  define SIZEOF_UV_ENV_ITEM_T          16
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_T             8
#  define SIZEOF_UV_MUTEX_T             40
#  define SIZEOF_UV_RWLOCK_T            56
#  define SIZEOF_UV_SEM_T               32
#  define SIZEOF_UV_COND_T              48
#  define SIZEOF_UV_ONCE_T               4
#  define SIZEOF_UV_KEY_T                4
#  define SIZEOF_UV_BARRIER_T           32
#  define SIZEOF_UV_THREAD_OPTIONS_T    16

# elif defined(__arm__)
#  define SIZEOF_UV_LOOP_T             472
#  define SIZEOF_UV_REQ_T               32
#  define SIZEOF_UV_HANDLE_T            48
#  define SIZEOF_UV_STREAM_T           132
#  define SIZEOF_UV_TCP_T              132
#  define SIZEOF_UV_UDP_T              112
#  define SIZEOF_UV_PIPE_T             140
#  define SIZEOF_UV_TTY_T              196
#  define SIZEOF_UV_POLL_T              84
#  define SIZEOF_UV_TIMER_T             88
#  define SIZEOF_UV_PREPARE_T           60
#  define SIZEOF_UV_CHECK_T             60
#  define SIZEOF_UV_IDLE_T              60
#  define SIZEOF_UV_ASYNC_T             64
#  define SIZEOF_UV_PROCESS_T           68
#  define SIZEOF_UV_FS_EVENT_T          68
#  define SIZEOF_UV_FS_POLL_T           52
#  define SIZEOF_UV_SIGNAL_T            80
#  define SIZEOF_UV_SHUTDOWN_T          40
#  define SIZEOF_UV_WRITE_T            100
#  define SIZEOF_UV_CONNECT_T           48
#  define SIZEOF_UV_UDP_SEND_T         224
#  define SIZEOF_UV_FS_T               296
#  define SIZEOF_UV_WORK_T              64
#  define SIZEOF_UV_GETADDRINFO_T       80
#  define SIZEOF_UV_GETNAMEINFO_T     1256
#  define SIZEOF_UV_RANDOM_T            72
#  define SIZEOF_UV_LIB_T                8
#  define SIZEOF_UV_BUF_T                8
#  define SIZEOF_UV_DIR_T               28
#  define SIZEOF_UV_STAT_T             128
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T           8
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T            8
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           128
#  define SIZEOF_UV_METRICS_T           80
#  define SIZEOF_UV_DIRENT_T             8
#  define SIZEOF_UV_PASSWD_T            20
#  define SIZEOF_UV_GROUP_T             12
#  define SIZEOF_UV_CPU_INFO_T          48
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 72
#  define SIZEOF_UV_ENV_ITEM_T           8
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_T             4
#  define SIZEOF_UV_MUTEX_T             24
#  define SIZEOF_UV_RWLOCK_T            32
#  define SIZEOF_UV_SEM_T               16
#  define SIZEOF_UV_COND_T              48
#  define SIZEOF_UV_ONCE_T               4
#  define SIZEOF_UV_KEY_T                4
#  define SIZEOF_UV_BARRIER_T           20
#  define SIZEOF_UV_THREAD_OPTIONS_T     8

# else
#  define UV_SIZEOF_SKIP "Linux: sizes not recorded for this arch/pointer-size"
# endif /* arch */

#elif defined(__APPLE__)

# if __SIZEOF_POINTER__ == 8
#  define SIZEOF_UV_LOOP_T            1072
#  define SIZEOF_UV_REQ_T               64
#  define SIZEOF_UV_HANDLE_T            96
#  define SIZEOF_UV_STREAM_T           264
#  define SIZEOF_UV_TCP_T              264
#  define SIZEOF_UV_UDP_T              224
#  define SIZEOF_UV_PIPE_T             280
#  define SIZEOF_UV_TTY_T              344
#  define SIZEOF_UV_POLL_T             168
#  define SIZEOF_UV_TIMER_T            152
#  define SIZEOF_UV_PREPARE_T          120
#  define SIZEOF_UV_CHECK_T            120
#  define SIZEOF_UV_IDLE_T             120
#  define SIZEOF_UV_ASYNC_T            128
#  define SIZEOF_UV_PROCESS_T          136
#  define SIZEOF_UV_FS_EVENT_T         304
#  define SIZEOF_UV_FS_POLL_T          104
#  define SIZEOF_UV_SIGNAL_T           152
#  define SIZEOF_UV_SHUTDOWN_T          80
#  define SIZEOF_UV_WRITE_T            192
#  define SIZEOF_UV_CONNECT_T           96
#  define SIZEOF_UV_UDP_SEND_T         320
#  define SIZEOF_UV_FS_T               440
#  define SIZEOF_UV_WORK_T             128
#  define SIZEOF_UV_GETADDRINFO_T      160
#  define SIZEOF_UV_GETNAMEINFO_T     1320
#  define SIZEOF_UV_RANDOM_T           144
#  define SIZEOF_UV_LIB_T               16
#  define SIZEOF_UV_BUF_T               16
#  define SIZEOF_UV_DIR_T               56
#  define SIZEOF_UV_STAT_T             160
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T          16
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T           16
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           144
#  define SIZEOF_UV_METRICS_T          128
#  define SIZEOF_UV_DIRENT_T            16
#  define SIZEOF_UV_PASSWD_T            40
#  define SIZEOF_UV_GROUP_T             24
#  define SIZEOF_UV_CPU_INFO_T          56
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 80
#  define SIZEOF_UV_ENV_ITEM_T          16
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_T             8
#  define SIZEOF_UV_MUTEX_T             64
#  define SIZEOF_UV_RWLOCK_T           200
#  define SIZEOF_UV_SEM_T                4
#  define SIZEOF_UV_COND_T              48
#  define SIZEOF_UV_ONCE_T              16
#  define SIZEOF_UV_KEY_T                8
#  define SIZEOF_UV_BARRIER_T            8
#  define SIZEOF_UV_THREAD_OPTIONS_T    16
# else
#  define UV_SIZEOF_SKIP "Darwin: unknown pointer size"
# endif

#elif defined(__FreeBSD__) || defined(__OpenBSD__) || \
      defined(__NetBSD__)  || defined(__DragonFly__)
//  These skip all thread types since pthread_t/mutex/cond types vary
//  significantly across FreeBSD, OpenBSD, NetBSD, and DragonFly.

# if defined(__x86_64__)
#  define SIZEOF_UV_LOOP_T             712
#  define SIZEOF_UV_REQ_T               64
#  define SIZEOF_UV_HANDLE_T            96
#  define SIZEOF_UV_STREAM_T           256
#  define SIZEOF_UV_TCP_T              256
#  define SIZEOF_UV_UDP_T              224
#  define SIZEOF_UV_PIPE_T             272
#  define SIZEOF_UV_TTY_T              304
#  define SIZEOF_UV_POLL_T             168
#  define SIZEOF_UV_TIMER_T            152
#  define SIZEOF_UV_PREPARE_T          120
#  define SIZEOF_UV_CHECK_T            120
#  define SIZEOF_UV_IDLE_T             120
#  define SIZEOF_UV_ASYNC_T            128
#  define SIZEOF_UV_PROCESS_T          136
#  define SIZEOF_UV_FS_EVENT_T         176
#  define SIZEOF_UV_FS_POLL_T          104
#  define SIZEOF_UV_SIGNAL_T           152
#  define SIZEOF_UV_SHUTDOWN_T          80
#  define SIZEOF_UV_WRITE_T            192
#  define SIZEOF_UV_CONNECT_T           96
#  define SIZEOF_UV_UDP_SEND_T         320
#  define SIZEOF_UV_FS_T               440
#  define SIZEOF_UV_WORK_T             128
#  define SIZEOF_UV_GETADDRINFO_T      160
#  define SIZEOF_UV_GETNAMEINFO_T     1320
#  define SIZEOF_UV_RANDOM_T           144
#  define SIZEOF_UV_LIB_T               16
#  define SIZEOF_UV_BUF_T               16
#  define SIZEOF_UV_DIR_T               56
#  define SIZEOF_UV_STAT_T             160
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T          16
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T           16
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           144
#  define SIZEOF_UV_METRICS_T          128
#  define SIZEOF_UV_DIRENT_T            16
#  define SIZEOF_UV_PASSWD_T            40
#  define SIZEOF_UV_GROUP_T             24
#  define SIZEOF_UV_CPU_INFO_T          56
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 80
#  define SIZEOF_UV_ENV_ITEM_T          16
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_OPTIONS_T    16
# elif defined(__i386__)
#  define SIZEOF_UV_LOOP_T             392
#  define SIZEOF_UV_REQ_T               32
#  define SIZEOF_UV_HANDLE_T            48
#  define SIZEOF_UV_STREAM_T           132
#  define SIZEOF_UV_TCP_T              132
#  define SIZEOF_UV_UDP_T              112
#  define SIZEOF_UV_PIPE_T             140
#  define SIZEOF_UV_TTY_T              176
#  define SIZEOF_UV_POLL_T              92
#  define SIZEOF_UV_TIMER_T             88
#  define SIZEOF_UV_PREPARE_T           60
#  define SIZEOF_UV_CHECK_T             60
#  define SIZEOF_UV_IDLE_T              60
#  define SIZEOF_UV_ASYNC_T             64
#  define SIZEOF_UV_PROCESS_T           68
#  define SIZEOF_UV_FS_EVENT_T         100
#  define SIZEOF_UV_FS_POLL_T           52
#  define SIZEOF_UV_SIGNAL_T            80
#  define SIZEOF_UV_SHUTDOWN_T          40
#  define SIZEOF_UV_WRITE_T            100
#  define SIZEOF_UV_CONNECT_T           48
#  define SIZEOF_UV_UDP_SEND_T         216
#  define SIZEOF_UV_FS_T               280
#  define SIZEOF_UV_WORK_T              64
#  define SIZEOF_UV_GETADDRINFO_T       80
#  define SIZEOF_UV_GETNAMEINFO_T     1256
#  define SIZEOF_UV_RANDOM_T            72
#  define SIZEOF_UV_LIB_T                8
#  define SIZEOF_UV_BUF_T                8
#  define SIZEOF_UV_DIR_T               28
#  define SIZEOF_UV_STAT_T             128
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T           8
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T            8
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           128
#  define SIZEOF_UV_METRICS_T           76
#  define SIZEOF_UV_DIRENT_T             8
#  define SIZEOF_UV_PASSWD_T            20
#  define SIZEOF_UV_GROUP_T             12
#  define SIZEOF_UV_CPU_INFO_T          48
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 72
#  define SIZEOF_UV_ENV_ITEM_T           8
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_OPTIONS_T     8
# else
#  define UV_SIZEOF_SKIP "BSD: sizes not recorded for this arch/pointer-size"
# endif

#elif defined(_AIX)
# define UV_SIZEOF_SKIP "AIX: sizes not yet recorded"

#elif defined(__sun)
# define UV_SIZEOF_SKIP "Solaris: sizes not yet recorded"

#elif defined(_WIN64)
#  define SIZEOF_UV_LOOP_T             472
#  define SIZEOF_UV_HANDLE_T            96
#  define SIZEOF_UV_STREAM_T           272
#  define SIZEOF_UV_TCP_T              320
#  define SIZEOF_UV_UDP_T              424
#  define SIZEOF_UV_PIPE_T             576
#  define SIZEOF_UV_TTY_T              344
#  define SIZEOF_UV_POLL_T             416
#  define SIZEOF_UV_TIMER_T            160
#  define SIZEOF_UV_PREPARE_T          120
#  define SIZEOF_UV_CHECK_T            120
#  define SIZEOF_UV_IDLE_T             120
#  define SIZEOF_UV_ASYNC_T            224
#  define SIZEOF_UV_PROCESS_T          264
#  define SIZEOF_UV_FS_EVENT_T         272
#  define SIZEOF_UV_FS_POLL_T          104
#  define SIZEOF_UV_SIGNAL_T           264
#  define SIZEOF_UV_REQ_T              112
#  define SIZEOF_UV_SHUTDOWN_T         128
#  define SIZEOF_UV_WRITE_T            176
#  define SIZEOF_UV_CONNECT_T          128
#  define SIZEOF_UV_UDP_SEND_T         128
#  define SIZEOF_UV_FS_T               456
#  define SIZEOF_UV_WORK_T             176
#  define SIZEOF_UV_GETADDRINFO_T      216
#  define SIZEOF_UV_GETNAMEINFO_T     1368
#  define SIZEOF_UV_RANDOM_T           192
#  define SIZEOF_UV_BUF_T               16
#  define SIZEOF_UV_LIB_T               16
#  define SIZEOF_UV_DIR_T              656
#  define SIZEOF_UV_STAT_T             128
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T           8
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T            8
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           128
#  define SIZEOF_UV_METRICS_T          128
#  define SIZEOF_UV_DIRENT_T            16
#  define SIZEOF_UV_PASSWD_T            32
#  define SIZEOF_UV_GROUP_T             24
#  define SIZEOF_UV_CPU_INFO_T          56
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 80
#  define SIZEOF_UV_ENV_ITEM_T          16
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_T             8
#  define SIZEOF_UV_MUTEX_T             40
#  define SIZEOF_UV_RWLOCK_T            80
#  define SIZEOF_UV_SEM_T                8
#  define SIZEOF_UV_COND_T              64
#  define SIZEOF_UV_ONCE_T              16
#  define SIZEOF_UV_KEY_T                4
#  define SIZEOF_UV_BARRIER_T           64
#  define SIZEOF_UV_THREAD_OPTIONS_T    16

#elif defined(_WIN32)
#  define SIZEOF_UV_LOOP_T             264
#  define SIZEOF_UV_HANDLE_T            48
#  define SIZEOF_UV_STREAM_T           144
#  define SIZEOF_UV_TCP_T              168
#  define SIZEOF_UV_UDP_T              288
#  define SIZEOF_UV_PIPE_T             320
#  define SIZEOF_UV_TTY_T              196
#  define SIZEOF_UV_POLL_T             256
#  define SIZEOF_UV_TIMER_T             96
#  define SIZEOF_UV_PREPARE_T           60
#  define SIZEOF_UV_CHECK_T             60
#  define SIZEOF_UV_IDLE_T              60
#  define SIZEOF_UV_ASYNC_T            116
#  define SIZEOF_UV_PROCESS_T          136
#  define SIZEOF_UV_FS_EVENT_T         140
#  define SIZEOF_UV_FS_POLL_T           52
#  define SIZEOF_UV_SIGNAL_T           136
#  define SIZEOF_UV_REQ_T               60
#  define SIZEOF_UV_SHUTDOWN_T          68
#  define SIZEOF_UV_WRITE_T             92
#  define SIZEOF_UV_CONNECT_T           68
#  define SIZEOF_UV_UDP_SEND_T          68
#  define SIZEOF_UV_FS_T               312
#  define SIZEOF_UV_WORK_T              92
#  define SIZEOF_UV_GETADDRINFO_T      112
#  define SIZEOF_UV_GETNAMEINFO_T     1288
#  define SIZEOF_UV_RANDOM_T           100
#  define SIZEOF_UV_BUF_T                8
#  define SIZEOF_UV_LIB_T                8
#  define SIZEOF_UV_DIR_T              624
#  define SIZEOF_UV_STAT_T             128
#  define SIZEOF_UV_STATFS_T            88
#  define SIZEOF_UV_TIMESPEC_T           8
#  define SIZEOF_UV_TIMESPEC64_T        16
#  define SIZEOF_UV_TIMEVAL_T            8
#  define SIZEOF_UV_TIMEVAL64_T         16
#  define SIZEOF_UV_RUSAGE_T           128
#  define SIZEOF_UV_METRICS_T           80
#  define SIZEOF_UV_DIRENT_T             8
#  define SIZEOF_UV_PASSWD_T            20
#  define SIZEOF_UV_GROUP_T             12
#  define SIZEOF_UV_CPU_INFO_T          48
#  define SIZEOF_UV_INTERFACE_ADDRESS_T 72
#  define SIZEOF_UV_ENV_ITEM_T           8
#  define SIZEOF_UV_UTSNAME_T         1024
#  define SIZEOF_UV_THREAD_T             4
#  define SIZEOF_UV_MUTEX_T             24
#  define SIZEOF_UV_RWLOCK_T            48
#  define SIZEOF_UV_SEM_T                4
#  define SIZEOF_UV_COND_T              36
#  define SIZEOF_UV_ONCE_T               8
#  define SIZEOF_UV_KEY_T                4
#  define SIZEOF_UV_BARRIER_T           40
#  define SIZEOF_UV_THREAD_OPTIONS_T     8

#else
# define UV_SIZEOF_SKIP "unsupported platform"
#endif


#if defined(UV_SIZEOF_SKIP)
#define CHECK_SIZE(type, expected) \
  printf("#  define %-28s%4u\n", #expected, (unsigned) sizeof(type))
#else
#define CHECK_SIZE(type, expected) \
  ASSERT_EQ(sizeof(type), (expected))
#endif

TEST_IMPL(sizeof) {
  CHECK_SIZE(uv_loop_t,              SIZEOF_UV_LOOP_T);

  /* Handles */
  CHECK_SIZE(uv_handle_t,            SIZEOF_UV_HANDLE_T);
  CHECK_SIZE(uv_stream_t,            SIZEOF_UV_STREAM_T);
  CHECK_SIZE(uv_tcp_t,               SIZEOF_UV_TCP_T);
  CHECK_SIZE(uv_udp_t,               SIZEOF_UV_UDP_T);
  CHECK_SIZE(uv_pipe_t,              SIZEOF_UV_PIPE_T);
  CHECK_SIZE(uv_tty_t,               SIZEOF_UV_TTY_T);
  CHECK_SIZE(uv_poll_t,              SIZEOF_UV_POLL_T);
  CHECK_SIZE(uv_timer_t,             SIZEOF_UV_TIMER_T);
  CHECK_SIZE(uv_prepare_t,           SIZEOF_UV_PREPARE_T);
  CHECK_SIZE(uv_check_t,             SIZEOF_UV_CHECK_T);
  CHECK_SIZE(uv_idle_t,              SIZEOF_UV_IDLE_T);
  CHECK_SIZE(uv_async_t,             SIZEOF_UV_ASYNC_T);
  CHECK_SIZE(uv_process_t,           SIZEOF_UV_PROCESS_T);
  CHECK_SIZE(uv_fs_event_t,          SIZEOF_UV_FS_EVENT_T);
  CHECK_SIZE(uv_fs_poll_t,           SIZEOF_UV_FS_POLL_T);
  CHECK_SIZE(uv_signal_t,            SIZEOF_UV_SIGNAL_T);

  /* Requests */
  CHECK_SIZE(uv_req_t,               SIZEOF_UV_REQ_T);
  CHECK_SIZE(uv_shutdown_t,          SIZEOF_UV_SHUTDOWN_T);
  CHECK_SIZE(uv_write_t,             SIZEOF_UV_WRITE_T);
  CHECK_SIZE(uv_connect_t,           SIZEOF_UV_CONNECT_T);
  CHECK_SIZE(uv_udp_send_t,          SIZEOF_UV_UDP_SEND_T);
  CHECK_SIZE(uv_fs_t,                SIZEOF_UV_FS_T);
  CHECK_SIZE(uv_work_t,              SIZEOF_UV_WORK_T);
  CHECK_SIZE(uv_getaddrinfo_t,       SIZEOF_UV_GETADDRINFO_T);
  CHECK_SIZE(uv_getnameinfo_t,       SIZEOF_UV_GETNAMEINFO_T);
  CHECK_SIZE(uv_random_t,            SIZEOF_UV_RANDOM_T);

  /* Data types */
  CHECK_SIZE(uv_buf_t,               SIZEOF_UV_BUF_T);
  CHECK_SIZE(uv_lib_t,               SIZEOF_UV_LIB_T);
  CHECK_SIZE(uv_dir_t,               SIZEOF_UV_DIR_T);
  CHECK_SIZE(uv_stat_t,              SIZEOF_UV_STAT_T);
  CHECK_SIZE(uv_statfs_t,            SIZEOF_UV_STATFS_T);
  CHECK_SIZE(uv_timespec_t,          SIZEOF_UV_TIMESPEC_T);
  CHECK_SIZE(uv_timespec64_t,        SIZEOF_UV_TIMESPEC64_T);
  CHECK_SIZE(uv_timeval_t,           SIZEOF_UV_TIMEVAL_T);
  CHECK_SIZE(uv_timeval64_t,         SIZEOF_UV_TIMEVAL64_T);
  CHECK_SIZE(uv_rusage_t,            SIZEOF_UV_RUSAGE_T);
  CHECK_SIZE(uv_metrics_t,           SIZEOF_UV_METRICS_T);
  CHECK_SIZE(uv_dirent_t,            SIZEOF_UV_DIRENT_T);
  CHECK_SIZE(uv_passwd_t,            SIZEOF_UV_PASSWD_T);
  CHECK_SIZE(uv_group_t,             SIZEOF_UV_GROUP_T);
  CHECK_SIZE(uv_cpu_info_t,          SIZEOF_UV_CPU_INFO_T);
  CHECK_SIZE(uv_interface_address_t, SIZEOF_UV_INTERFACE_ADDRESS_T);
  CHECK_SIZE(uv_env_item_t,          SIZEOF_UV_ENV_ITEM_T);
  CHECK_SIZE(uv_utsname_t,           SIZEOF_UV_UTSNAME_T);

  /* Threading primitives */
#if defined(UV_SIZEOF_SKIP) || defined(SIZEOF_UV_MUTEX_T)
  CHECK_SIZE(uv_thread_t,            SIZEOF_UV_THREAD_T);
  CHECK_SIZE(uv_mutex_t,             SIZEOF_UV_MUTEX_T);
  CHECK_SIZE(uv_rwlock_t,            SIZEOF_UV_RWLOCK_T);
  CHECK_SIZE(uv_sem_t,               SIZEOF_UV_SEM_T);
  CHECK_SIZE(uv_cond_t,              SIZEOF_UV_COND_T);
  CHECK_SIZE(uv_once_t,              SIZEOF_UV_ONCE_T);
  CHECK_SIZE(uv_key_t,               SIZEOF_UV_KEY_T);
  CHECK_SIZE(uv_barrier_t,           SIZEOF_UV_BARRIER_T);
#endif
  CHECK_SIZE(uv_thread_options_t,    SIZEOF_UV_THREAD_OPTIONS_T);

#if defined(UV_SIZEOF_SKIP)
  RETURN_SKIP(UV_SIZEOF_SKIP);
#else
  return 0;
#endif
}
