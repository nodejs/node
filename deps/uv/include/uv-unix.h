/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

#ifndef UV_UNIX_H
#define UV_UNIX_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <termios.h>
#include <pwd.h>

#include <semaphore.h>
#include <pthread.h>
#ifdef __ANDROID__
#include "pthread-fixes.h"
#endif
#include <signal.h>

#if defined(__linux__)
# include "uv-linux.h"
#elif defined(__sun)
# include "uv-sunos.h"
#elif defined(__APPLE__)
# include "uv-darwin.h"
#elif defined(__DragonFly__)  || \
      defined(__FreeBSD__)    || \
      defined(__OpenBSD__)    || \
      defined(__NetBSD__)
# include "uv-bsd.h"
#endif

#ifndef UV_IO_PRIVATE_PLATFORM_FIELDS
# define UV_IO_PRIVATE_PLATFORM_FIELDS /* empty */
#endif

struct uv__io_s;
struct uv__async;
struct uv_loop_s;

typedef void (*uv__io_cb)(struct uv_loop_s* loop,
                          struct uv__io_s* w,
                          unsigned int events);
typedef struct uv__io_s uv__io_t;

struct uv__io_s {
  uv__io_cb cb;
  void* pending_queue[2];
  void* watcher_queue[2];
  unsigned int pevents; /* Pending event mask i.e. mask at next tick. */
  unsigned int events;  /* Current event mask. */
  int fd;
  UV_IO_PRIVATE_PLATFORM_FIELDS
};

typedef void (*uv__async_cb)(struct uv_loop_s* loop,
                             struct uv__async* w,
                             unsigned int nevents);

struct uv__async {
  uv__async_cb cb;
  uv__io_t io_watcher;
  int wfd;
};

struct uv__work {
  void (*work)(struct uv__work *w);
  void (*done)(struct uv__work *w, int status);
  struct uv_loop_s* loop;
  void* wq[2];
};

#ifndef UV_PLATFORM_SEM_T
# define UV_PLATFORM_SEM_T sem_t
#endif

#ifndef UV_PLATFORM_LOOP_FIELDS
# define UV_PLATFORM_LOOP_FIELDS /* empty */
#endif

#ifndef UV_PLATFORM_FS_EVENT_FIELDS
# define UV_PLATFORM_FS_EVENT_FIELDS /* empty */
#endif

#ifndef UV_STREAM_PRIVATE_PLATFORM_FIELDS
# define UV_STREAM_PRIVATE_PLATFORM_FIELDS /* empty */
#endif

/* Note: May be cast to struct iovec. See writev(2). */
typedef struct {
  char* base;
  size_t len;
} uv_buf_t;

typedef int uv_file;
typedef int uv_os_sock_t;

#define UV_ONCE_INIT PTHREAD_ONCE_INIT

typedef pthread_once_t uv_once_t;
typedef pthread_t uv_thread_t;
typedef pthread_mutex_t uv_mutex_t;
typedef pthread_rwlock_t uv_rwlock_t;
typedef UV_PLATFORM_SEM_T uv_sem_t;
typedef pthread_cond_t uv_cond_t;
typedef pthread_key_t uv_key_t;

#if defined(__APPLE__) && defined(__MACH__)

typedef struct {
  unsigned int n;
  unsigned int count;
  uv_mutex_t mutex;
  uv_sem_t turnstile1;
  uv_sem_t turnstile2;
} uv_barrier_t;

#else /* defined(__APPLE__) && defined(__MACH__) */

typedef pthread_barrier_t uv_barrier_t;

#endif /* defined(__APPLE__) && defined(__MACH__) */

/* Platform-specific definitions for uv_spawn support. */
typedef gid_t uv_gid_t;
typedef uid_t uv_uid_t;

/* Platform-specific definitions for uv_dlopen support. */
#define UV_DYNAMIC /* empty */

typedef struct {
  void* handle;
  char* errmsg;
} uv_lib_t;

#define UV_LOOP_PRIVATE_FIELDS                                                \
  unsigned long flags;                                                        \
  int backend_fd;                                                             \
  void* pending_queue[2];                                                     \
  void* watcher_queue[2];                                                     \
  uv__io_t** watchers;                                                        \
  unsigned int nwatchers;                                                     \
  unsigned int nfds;                                                          \
  void* wq[2];                                                                \
  uv_mutex_t wq_mutex;                                                        \
  uv_async_t wq_async;                                                        \
  uv_handle_t* closing_handles;                                               \
  void* process_handles[1][2];                                                \
  void* prepare_handles[2];                                                   \
  void* check_handles[2];                                                     \
  void* idle_handles[2];                                                      \
  void* async_handles[2];                                                     \
  struct uv__async async_watcher;                                             \
  /* RB_HEAD(uv__timers, uv_timer_s) */                                       \
  struct uv__timers {                                                         \
    struct uv_timer_s* rbh_root;                                              \
  } timer_handles;                                                            \
  uint64_t time;                                                              \
  int signal_pipefd[2];                                                       \
  uv__io_t signal_io_watcher;                                                 \
  uv_signal_t child_watcher;                                                  \
  int emfile_fd;                                                              \
  uint64_t timer_counter;                                                     \
  UV_PLATFORM_LOOP_FIELDS                                                     \

#define UV_REQ_TYPE_PRIVATE /* empty */

#define UV_REQ_PRIVATE_FIELDS  /* empty */

#define UV_PRIVATE_REQ_TYPES /* empty */

#define UV_WRITE_PRIVATE_FIELDS                                               \
  void* queue[2];                                                             \
  unsigned int write_index;                                                   \
  uv_buf_t* bufs;                                                             \
  unsigned int nbufs;                                                         \
  int error;                                                                  \
  uv_buf_t bufsml[4];                                                         \

#define UV_CONNECT_PRIVATE_FIELDS                                             \
  void* queue[2];                                                             \

#define UV_SHUTDOWN_PRIVATE_FIELDS /* empty */

#define UV_UDP_SEND_PRIVATE_FIELDS                                            \
  void* queue[2];                                                             \
  struct sockaddr_in6 addr;                                                   \
  unsigned int nbufs;                                                         \
  uv_buf_t* bufs;                                                             \
  ssize_t status;                                                             \
  uv_udp_send_cb send_cb;                                                     \
  uv_buf_t bufsml[4];                                                         \

#define UV_HANDLE_PRIVATE_FIELDS                                              \
  int flags;                                                                  \
  uv_handle_t* next_closing;                                                  \

#define UV_STREAM_PRIVATE_FIELDS                                              \
  uv_connect_t *connect_req;                                                  \
  uv_shutdown_t *shutdown_req;                                                \
  uv__io_t io_watcher;                                                        \
  void* write_queue[2];                                                       \
  void* write_completed_queue[2];                                             \
  uv_connection_cb connection_cb;                                             \
  int delayed_error;                                                          \
  int accepted_fd;                                                            \
  UV_STREAM_PRIVATE_PLATFORM_FIELDS                                           \

#define UV_TCP_PRIVATE_FIELDS /* empty */

#define UV_UDP_PRIVATE_FIELDS                                                 \
  uv_alloc_cb alloc_cb;                                                       \
  uv_udp_recv_cb recv_cb;                                                     \
  uv__io_t io_watcher;                                                        \
  void* write_queue[2];                                                       \
  void* write_completed_queue[2];                                             \

#define UV_PIPE_PRIVATE_FIELDS                                                \
  const char* pipe_fname; /* strdup'ed */

#define UV_POLL_PRIVATE_FIELDS                                                \
  uv__io_t io_watcher;

#define UV_PREPARE_PRIVATE_FIELDS                                             \
  uv_prepare_cb prepare_cb;                                                   \
  void* queue[2];                                                             \

#define UV_CHECK_PRIVATE_FIELDS                                               \
  uv_check_cb check_cb;                                                       \
  void* queue[2];                                                             \

#define UV_IDLE_PRIVATE_FIELDS                                                \
  uv_idle_cb idle_cb;                                                         \
  void* queue[2];                                                             \

#define UV_ASYNC_PRIVATE_FIELDS                                               \
  uv_async_cb async_cb;                                                       \
  void* queue[2];                                                             \
  int pending;                                                                \

#define UV_TIMER_PRIVATE_FIELDS                                               \
  /* RB_ENTRY(uv_timer_s) tree_entry; */                                      \
  struct {                                                                    \
    struct uv_timer_s* rbe_left;                                              \
    struct uv_timer_s* rbe_right;                                             \
    struct uv_timer_s* rbe_parent;                                            \
    int rbe_color;                                                            \
  } tree_entry;                                                               \
  uv_timer_cb timer_cb;                                                       \
  uint64_t timeout;                                                           \
  uint64_t repeat;                                                            \
  uint64_t start_id;

#define UV_GETADDRINFO_PRIVATE_FIELDS                                         \
  struct uv__work work_req;                                                   \
  uv_getaddrinfo_cb cb;                                                       \
  struct addrinfo* hints;                                                     \
  char* hostname;                                                             \
  char* service;                                                              \
  struct addrinfo* res;                                                       \
  int retcode;

#define UV_PROCESS_PRIVATE_FIELDS                                             \
  void* queue[2];                                                             \
  int errorno;                                                                \
  int status;                                                                 \

#define UV_FS_PRIVATE_FIELDS                                                  \
  const char *new_path;                                                       \
  uv_file file;                                                               \
  int flags;                                                                  \
  mode_t mode;                                                                \
  void* buf;                                                                  \
  size_t len;                                                                 \
  off_t off;                                                                  \
  uv_uid_t uid;                                                               \
  uv_gid_t gid;                                                               \
  double atime;                                                               \
  double mtime;                                                               \
  struct uv__work work_req;                                                   \

#define UV_WORK_PRIVATE_FIELDS                                                \
  struct uv__work work_req;

#define UV_TTY_PRIVATE_FIELDS                                                 \
  struct termios orig_termios;                                                \
  int mode;

#define UV_SIGNAL_PRIVATE_FIELDS                                              \
  /* RB_ENTRY(uv_signal_s) tree_entry; */                                     \
  struct {                                                                    \
    struct uv_signal_s* rbe_left;                                             \
    struct uv_signal_s* rbe_right;                                            \
    struct uv_signal_s* rbe_parent;                                           \
    int rbe_color;                                                            \
  } tree_entry;                                                               \
  /* Use two counters here so we don have to fiddle with atomics. */          \
  unsigned int caught_signals;                                                \
  unsigned int dispatched_signals;

#define UV_FS_EVENT_PRIVATE_FIELDS                                            \
  uv_fs_event_cb cb;                                                          \
  UV_PLATFORM_FS_EVENT_FIELDS                                                 \

#endif /* UV_UNIX_H */
