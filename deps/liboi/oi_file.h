#include <oi.h>
#include <ev.h>
#ifdef __cplusplus
extern "C" {
#endif 

#ifndef oi_file_h
#define oi_file_h

typedef struct oi_file oi_file;

int  oi_file_init         (oi_file *);

void oi_file_attach       (oi_file *, struct ev_loop *);
void oi_file_detach       (oi_file *);

/* WARNING oi_file_open_path: path argument must be valid until oi_file
 * object is closed and the on_close() callback is made. oi does not strdup
 * the path pointer. */
int  oi_file_open_path    (oi_file *, const char *path, int flags, mode_t mode);
int  oi_file_open_stdin   (oi_file *);
int  oi_file_open_stdout  (oi_file *);
int  oi_file_open_stderr  (oi_file *);

void oi_file_read_start   (oi_file *, void *buffer, size_t bufsize);
void oi_file_read_stop    (oi_file *);
int  oi_file_write        (oi_file *, oi_buf *to_write);
int  oi_file_write_simple (oi_file *, const char *, size_t);
int  oi_file_send         (oi_file *source, oi_socket *destination, off_t offset, size_t length);
void oi_file_close        (oi_file *);

struct oi_file {
  /* private */
  oi_async async;
  oi_task io_task;
  struct ev_loop *loop;
  oi_queue write_queue;
  oi_buf *write_buf; /* TODO this pointer is unnecessary - remove and just look at first element of the queue */
  oi_socket *write_socket;
  void *read_buffer;
  size_t read_buffer_size;

  /* read-only */
  int fd;
   
  /* public */
  void (*on_open)  (oi_file *);
  void (*on_read)  (oi_file *, size_t count);
  void (*on_drain) (oi_file *);
  void (*on_error) (oi_file *, struct oi_error);
  void (*on_close) (oi_file *);
  void *data;
};

#ifdef __cplusplus
}
#endif 
#endif /*  oi_file_h */
