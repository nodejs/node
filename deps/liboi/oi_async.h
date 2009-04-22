#include <ev.h>
#include <pthread.h>
#include <netdb.h>
#include <oi.h>

#ifndef oi_async_h
#define oi_async_h
#ifdef __cplusplus
extern "C" {
#endif 

typedef struct oi_async oi_async;
typedef struct oi_task  oi_task;

struct oi_async {
  /* private */
  ev_async watcher;  
  struct ev_loop *loop;

  oi_queue finished_tasks;
  oi_queue new_tasks;

  /* public */
  void *data;
}; 

typedef void (*oi_task_int_cb)(oi_task *, int result);
typedef void (*oi_task_uint_cb)(oi_task *, unsigned int result);
typedef void (*oi_task_ssize_cb)(oi_task *, ssize_t result);

struct oi_task {
  /* private */
  oi_async *async;
  oi_queue queue;
  int type;
  union {

    struct {
      const char *pathname;
      int flags;
      mode_t mode;
      oi_task_int_cb cb;
      int result;
    } open;

    struct {
      int fd;
      void *buf;
      size_t count;
      oi_task_ssize_cb cb;
      ssize_t result;
    } read;

    struct {
      int fd;
      const void *buf;
      size_t count;
      oi_task_ssize_cb cb;
      ssize_t result;
    } write;

    struct {
      int fd;
      oi_task_int_cb cb;
      int result;
    } close;

    struct {
      unsigned int seconds;
      oi_task_uint_cb cb;
      unsigned int result;
    } sleep;

    struct {
      int out_fd;
      int in_fd;
      off_t offset;
      size_t count;
      oi_task_ssize_cb cb;
      ssize_t result;
    } eio__sendfile;

    struct {
      const char *nodename; /* restrict ? */
      const char *servname; /* restrict ? */
      struct addrinfo *hints;
      struct addrinfo **res; /* restrict ? */
      oi_task_int_cb cb;
      int result;
    } getaddrinfo;

    struct {
      const char *path;
      struct stat *buf;
      oi_task_int_cb cb;
      int result;
    } lstat;
    
  } params;

  /* read-only */
  volatile unsigned active:1;
  int errorno;

  /* public */
  void *data;
}; 

void oi_async_init    (oi_async *);
void oi_async_attach  (struct ev_loop *loop, oi_async *);
void oi_async_detach  (oi_async *);
void oi_async_submit  (oi_async *, oi_task *);

/* To submit a task for async processing
 * (0) allocate memory for your task
 * (1) initialize the task with one of the functions below
 * (2) optionally set the task->data pointer
 * (3) oi_async_submit() the task 
 */

enum { OI_TASK_OPEN
     , OI_TASK_READ
     , OI_TASK_WRITE
     , OI_TASK_CLOSE
     , OI_TASK_SLEEP
     , OI_TASK_SENDFILE
     , OI_TASK_GETADDRINFO
     , OI_TASK_LSTAT
     };

#define oi_task_init_common(task, _type) do {\
  (task)->active = 0;\
  (task)->async = NULL;\
  (task)->type = _type;\
} while(0)

static inline void 
oi_task_init_open(oi_task *t, oi_task_int_cb cb, const char *pathname, int flags, mode_t mode) 
{
  oi_task_init_common(t, OI_TASK_OPEN);
  t->params.open.cb = cb;
  t->params.open.pathname = pathname;
  t->params.open.flags = flags;
  t->params.open.mode = mode;
}

static inline void 
oi_task_init_read(oi_task *t, oi_task_ssize_cb cb, int fd, void *buf, size_t count) 
{
  oi_task_init_common(t, OI_TASK_READ);
  t->params.read.cb = cb;
  t->params.read.fd = fd;
  t->params.read.buf = buf;
  t->params.read.count = count;
}

static inline void 
oi_task_init_write(oi_task *t, oi_task_ssize_cb cb, int fd, const void *buf, size_t count) 
{
  oi_task_init_common(t, OI_TASK_WRITE);
  t->params.write.cb = cb;
  t->params.write.fd = fd;
  t->params.write.buf = buf;
  t->params.write.count = count;
}

static inline void 
oi_task_init_close(oi_task *t, oi_task_int_cb cb, int fd) 
{
  oi_task_init_common(t, OI_TASK_CLOSE);
  t->params.close.cb = cb;
  t->params.close.fd = fd;
}

static inline void 
oi_task_init_sleep(oi_task *t, oi_task_uint_cb cb, unsigned int seconds) 
{
  oi_task_init_common(t, OI_TASK_SLEEP);
  t->params.sleep.cb = cb;
  t->params.sleep.seconds = seconds;
}

static inline void 
oi_task_init_sendfile(oi_task *t, oi_task_ssize_cb cb, int out_fd, int in_fd, off_t offset, size_t count) 
{
  oi_task_init_common(t, OI_TASK_SENDFILE);
  t->params.eio__sendfile.cb = cb;
  t->params.eio__sendfile.out_fd = out_fd;
  t->params.eio__sendfile.in_fd = in_fd;
  t->params.eio__sendfile.offset = offset;
  t->params.eio__sendfile.count = count;
}

static inline void 
oi_task_init_getaddrinfo(oi_task *t, oi_task_int_cb cb, const char *node, 
                          const char *service, struct addrinfo *hints, struct addrinfo **res) 
{
  oi_task_init_common(t, OI_TASK_GETADDRINFO);
  t->params.getaddrinfo.cb = cb;
  t->params.getaddrinfo.nodename = node;
  t->params.getaddrinfo.servname = service;
  t->params.getaddrinfo.hints = hints;
  t->params.getaddrinfo.res = res;
}

static inline void 
oi_task_init_lstat(oi_task *t, oi_task_int_cb cb, const char *path, struct stat *buf) 
{
  oi_task_init_common(t, OI_TASK_LSTAT);
  t->params.lstat.cb = cb;
  t->params.lstat.path = path;
  t->params.lstat.buf = buf;
}

#ifdef __cplusplus
}
#endif 
#endif /* oi_async_h */
