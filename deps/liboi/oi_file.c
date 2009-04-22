#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include <ev.h>
#include <oi.h>

#define RELEASE_BUF(buf) if(buf->release) { buf->release(buf); }
#define DRAIN_CB(file)   if(file->on_drain) { file->on_drain(file); }
#define RAISE_ERROR(s, _domain, _code) do { \
  if(s->on_error) { \
    struct oi_error __oi_error; \
    __oi_error.domain = _domain; \
    __oi_error.code = _code; \
    s->on_error(s, __oi_error); \
  } \
} while(0) \

/* forwards */
static void dispatch_write_buf (oi_file *file);
static void maybe_do_read (oi_file *file);

static void
after_read(oi_task *task, ssize_t recved)
{
  oi_file *file = task->data;

  if(recved == -1) {
    RAISE_ERROR(file, OI_ERROR_READ, errno);
    return;
  }

  if(recved == 0)
    oi_file_read_stop(file);

  if(file->on_read)
    file->on_read(file, recved);

  maybe_do_read(file);
}

static void
maybe_do_read(oi_file *file)
{
  if ( file->read_buffer == NULL
    || file->write_buf != NULL
    || file->write_socket != NULL
    || !oi_queue_empty(&file->write_queue)
    || file->io_task.active
     ) return;

  assert(file->fd > 0);

  oi_task_init_read ( &file->io_task
                    , after_read
                    , file->fd
                    , file->read_buffer
                    , file->read_buffer_size
                    );
  file->io_task.data = file;
  oi_async_submit(&file->async, &file->io_task);
}

static void 
submit_read (oi_file *file)
{
}

int 
oi_file_init (oi_file *file)
{
  oi_async_init(&file->async);
  file->async.data = file;

  oi_queue_init(&file->write_queue);

  file->fd = -1;
  file->loop = NULL;
  file->write_buf = NULL;
  file->read_buffer = NULL;

  file->on_open = NULL;
  file->on_read = NULL;
  file->on_drain = NULL;
  file->on_error = NULL;
  file->on_close = NULL;
  return 0;
}

void
oi_file_read_start (oi_file *file, void *buffer, size_t bufsize)
{
  file->read_buffer = buffer;
  file->read_buffer_size = bufsize;
  maybe_do_read(file);
}

void
oi_file_read_stop (oi_file *file)
{
  file->read_buffer = NULL;
}

void
oi_api_free_buf_with_heap_base(oi_buf *buf)
{
  free(buf->base);
  free(buf);
}

static void 
after_open(oi_task *task, int result)
{
  oi_file *file = task->data;

  if(result == -1) {
    RAISE_ERROR(file, OI_ERROR_OPEN, errno);
    return;
  }

  file->fd = result;

  if(file->on_open) {
    file->on_open(file);
  }

  maybe_do_read(file);
}

int
oi_file_open_path (oi_file *file, const char *path, int flags, mode_t mode)
{
  if(file->fd >= 0)
    return -1;
  oi_task_init_open( &file->io_task
                   , after_open
                   , path
                   , flags
                   , mode 
                   );
  file->io_task.data = file;
  oi_async_submit(&file->async, &file->io_task);
  return 0;
}

int
oi_file_open_stdin (oi_file *file)
{
  if(file->fd >= 0)
    return -1;
  file->fd = STDIN_FILENO;
  if(file->on_open)
    file->on_open(file);
  return 0;
}

int
oi_file_open_stdout (oi_file *file)
{
  if(file->fd >= 0)
    return -1;
  file->fd = STDOUT_FILENO;
  if(file->on_open)
    file->on_open(file);
  return 0;
}

int
oi_file_open_stderr (oi_file *file)
{
  if(file->fd >= 0)
    return -1;
  file->fd = STDERR_FILENO;
  if(file->on_open)
    file->on_open(file);
  return 0;
}

void
oi_file_attach (oi_file *file, struct ev_loop *loop)
{
  oi_async_attach (loop, &file->async);
  file->loop = loop;
}

void
oi_file_detach (oi_file *file)
{
  oi_async_detach (&file->async);
  file->loop = NULL;
}

static void 
after_write(oi_task *task, ssize_t result)
{
  oi_file *file = task->data;

  if(result == -1) {
    RAISE_ERROR(file, OI_ERROR_WRITE, errno);
    return;
  }

  assert(file->write_buf != NULL);
  oi_buf *buf = file->write_buf;

  buf->written += result;
  if(buf->written < buf->len) {
    oi_task_init_write ( &file->io_task
                       , after_write
                       , file->fd
                       , buf->base + buf->written
                       , buf->len - buf->written
                       );
    file->io_task.data = file;
    oi_async_submit(&file->async, &file->io_task);
    return;
  }

  assert(buf->written == buf->len);

  RELEASE_BUF(file->write_buf);
  file->write_buf = NULL;

  if(oi_queue_empty(&file->write_queue)) {
    DRAIN_CB(file);
    maybe_do_read(file);
  } else {
    dispatch_write_buf(file);
  }

  return;
}

static void
dispatch_write_buf(oi_file *file)
{
  if(file->write_buf != NULL)
    return;
  if(oi_queue_empty(&file->write_queue)) 
    return;

  oi_queue *q = oi_queue_last(&file->write_queue);
  oi_queue_remove(q);
  oi_buf *buf = file->write_buf = oi_queue_data(q, oi_buf, queue);

  assert(!file->io_task.active);
  oi_task_init_write ( &file->io_task
                     , after_write
                     , file->fd
                     , buf->base + buf->written
                     , buf->len - buf->written
                     );
  file->io_task.data = file;
  oi_async_submit(&file->async, &file->io_task);
}

int
oi_file_write (oi_file *file, oi_buf *buf)
{
  if(file->fd < 0)
    return -1;
  if(file->read_buffer)
    return -2;
  /* TODO better business check*/

  buf->written = 0;
  oi_queue_insert_head(&file->write_queue, &buf->queue);
  dispatch_write_buf(file);

  return 0;
}

// Writes a string to the file. 
// NOTE: Allocates memory. Avoid for performance applications.
int
oi_file_write_simple (oi_file *file, const char *str, size_t len)
{
  if(file->fd < 0)
    return -1;
  if(file->read_buffer)
    return -2;
  /* TODO better business check*/

  oi_buf *buf = malloc(sizeof(oi_buf));
  buf->base = malloc(len);
  memcpy(buf->base, str, len);
  buf->len = len;
  buf->release = oi_api_free_buf_with_heap_base;

  oi_file_write(file, buf);
  return 0;
}

static void
clear_write_queue(oi_file *file)
{
  while(!oi_queue_empty(&file->write_queue)) {
    oi_queue *q = oi_queue_last(&file->write_queue);
    oi_queue_remove(q);
    oi_buf *buf = oi_queue_data(q, oi_buf, queue);
    RELEASE_BUF(buf);
  }
}

static void 
after_close(oi_task *task, int result)
{
  oi_file *file = task->data;

  assert(oi_queue_empty(&file->write_queue));

  if(result == -1) {
    RAISE_ERROR(file, OI_ERROR_CLOSE, errno);
    return;
    // TODO try to close again? 
  }

  file->fd = -1;
  // TODO deinit task_queue, detach thread_pool_result_watcher 

  if(file->on_close) {
    file->on_close(file);
  }

  return;
}

void
oi_file_close (oi_file *file)
{
  assert(file->fd >= 0 && "file not open!");
  clear_write_queue(file);
  oi_task_init_close ( &file->io_task
                     , after_close
                     , file->fd
                     );
  file->io_task.data = file;
  oi_async_submit(&file->async, &file->io_task);
}

static void
after_sendfile(oi_task *task, ssize_t sent)
{
  oi_file *file = task->data;
  oi_socket *socket = file->write_socket;
  assert(socket != NULL);
  file->write_socket = NULL;

  if(sent == -1) {
    RAISE_ERROR(file, OI_ERROR_SENDFILE, errno);
    return;
  }

  if(socket->on_drain) {
    socket->on_drain(socket);
  }

  maybe_do_read(file);
}

int
oi_file_send (oi_file *file, oi_socket *destination, off_t offset, size_t count)
{
  if(file->fd < 0)
    return -1;
  if(file->read_buffer)
    return -2;
  /* TODO better business check*/

  assert(file->write_socket == NULL);
  // (1) make sure the write queue on the socket is cleared.
  // 
  // (2)
  // 
  file->write_socket = destination;
  oi_task_init_sendfile ( &file->io_task
                        , after_sendfile
                        , destination->fd
                        , file->fd
                        , offset
                        , count
                        );
  file->io_task.data = file;
  oi_async_submit(&file->async, &file->io_task);

  return 0;
}

