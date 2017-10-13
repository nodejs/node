#ifndef SRC_NODE_IO_H_
#define SRC_NODE_IO_H_

#include "node.h"

namespace node {

namespace io {

enum io_error {
  // Source would overflow the provided buffer
  IO_ERROR_OVERFLOW = -256,
  // Source does not support FLAG_PEEK
  IO_ERROR_NO_PEEK = -512,
  // Attempt to pull from an already ended source
  IO_ERROR_EOF = -768,
  // Source will not copy data into caller provided buffer
  IO_ERROR_MUST_NO_COPY = -1024,
};

enum io_pull_flags {
  IO_PULL_FLAG_NONE = 0x0,
  // Callback must be invoked synchronously (also sets the
  // IO_PULL_FLAG_MUST_COPY and IO_PULL_FLAG_STRICT_LENGTH flags)
  IO_PULL_FLAG_SYNC = 0xD,
  // Peek at what data is available, do not actually read.
  IO_PULL_FLAG_PEEK = 0x2,
  // Puller considers the length given to be strict,
  // Source must not overflow
  IO_PULL_FLAG_STRICT_LENGTH = 0x4,
  // Puller requires source to copy data into the provided buffer
  // (also sets the IO_PULL_FLAG_STRICT_LENGTH flag)
  IO_PULL_FLAG_MUST_COPY = 0xC
};

enum io_bind_flags {
  IO_BIND_FLAG_NONE = 0x0,
  // Signal the Sink immediately after bind
  IO_BIND_FLAG_SIGNAL = 0x1,
  // Source supports peek
  IO_BIND_FLAG_PEEK = 0x2,
  // Pull must be performed synchronously
  IO_BIND_FLAG_SYNC = 0x3
};

enum io_unbind_flags {
  IO_UNBIND_FLAG_NONE = 0x0,
  IO_UNBIND_FLAG_IMMEDIATE = 0x1
};

enum io_pull_status {
  IO_PULL_STATUS_OK = 0x0,
  // After this call, wait before asking again
  IO_PULL_STATUS_WAIT = 0x1,
  // There was an error processing the pull request
  IO_PULL_STATUS_ERROR = 0x2,
  // After this call, there is no more data
  IO_PULL_STATUS_END = 0x4,
  // The Source owns the buffer and did not
  // copy into the Sink provided buffer. The
  // Sink must call the push_cb when it is
  // done using the data
  IO_PULL_STATUS_NO_COPY = 0x8,
  // Response is synchronous
  IO_PULL_STATUS_SYNC = 0x10
};

enum io_push_cb_status {
  IO_PUSH_STATUS_OK = 0x0,
  IO_PUSH_STATUS_CANCEL = 0x1,
  IO_PUSH_STATUS_ERROR = 0x2
};

enum io_bind_cb_status {
  IO_BIND_STATUS_OK = 0x0,
  IO_BIND_STATUS_WAIT = 0x1,
  IO_BIND_STATUS_ERROR = 0x2
};

enum io_unbind_cb_status {
  IO_UNBIND_STATUS_OK = 0x0,
  IO_UNBIND_STATUS_ERROR = 0x2
};

enum io_signal {
  IO_SIGNAL_START = 0x0,
  IO_SIGNAL_PAUSE = 0x1,
  IO_SIGNAL_STOP = 0x2
};

typedef struct io_pull_s io_pull_t;
typedef struct io_push_s io_push_t;
typedef struct io_bind_s io_bind_t;
typedef struct io_buf_s io_buf_t;

class Source;
class Sink;

typedef void (*bind_cb)(io_bind_t* handle,
                        int status);

typedef void (*unbind_cb)(io_bind_t* handle,
                          int status);

typedef void (*push_cb)(io_push_t* handle,
                        int status,
                        size_t length);

typedef void (*pull_cb)(io_pull_t* handle,
                        int status,
                        io_buf_t* bufs,
                        size_t count,
                        size_t length,
                        io_push_t* source);

typedef void (*alloc_cb)(io_bind_t* handle,
                         size_t length,
                         io_buf_t** bufs,
                         size_t* count);

typedef void (*free_cb)(io_bind_t* handle,
                        size_t length,
                        io_buf_t* bufs,
                        size_t count);

struct io_pull_s {
#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
  pull_cb pull = nullptr;
  void* user_data = nullptr;
#endif
};

struct io_push_s {
#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
  push_cb push = nullptr;
  void* user_data = nullptr;
#endif
};

struct io_bind_s {
#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
  bind_cb bind = nullptr;
  unbind_cb unbind = nullptr;
  alloc_cb alloc = nullptr;
  free_cb free = nullptr;
  void* user_data = nullptr;
#endif
};

// Essentially the same as a uv_buf_t, except uint8_t rather than char
struct io_buf_s {
  uint8_t* base;
  size_t len;
};


inline void io_bind_set_user_data(io_bind_t* handle, void* user_data);
inline void io_pull_set_user_data(io_pull_t* handle, void* user_data);
inline void io_push_set_user_data(io_push_t* handle, void* user_data);
inline void* io_bind_get_user_data(io_bind_t* handle);
inline void* io_pull_get_user_data(io_pull_t* handle);
inline void* io_push_get_user_data(io_push_t* handle);

inline void io_bind_set_alloc_cb(io_bind_t* handle, alloc_cb cb);
inline void io_bind_set_free_cb(io_bind_t* handle, free_cb cb);
inline void io_bind_set_bind_cb(io_bind_t* handle, bind_cb cb);
inline void io_bind_set_unbind_cb(io_bind_t* handle, unbind_cb cb);
inline void io_pull_set_pull_cb(io_pull_t* handle, pull_cb cb);
inline void io_push_set_push_cb(io_push_t* handle, push_cb cb);

inline alloc_cb io_bind_get_alloc_cb(io_bind_t* handle);
inline free_cb io_bind_get_free_cb(io_bind_t* handle);
inline bind_cb io_bind_get_bind_cb(io_bind_t* handle);
inline unbind_cb io_bind_get_unbind_cb(io_bind_t* handle);
inline pull_cb io_pull_get_pull_cb(io_pull_t* handle);
inline push_cb io_push_get_push_cb(io_push_t* handle);

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
inline void io_bind_set_source(io_bind_t* handle, Source* source);
inline void io_bind_set_sink(io_bind_t* handle, Sink* sink);
#endif

inline Source* io_bind_get_source(io_bind_t* handle);
inline Sink* io_bind_get_sink(io_bind_t* handle);

inline int io_get_error(int status);

class Source {
 public:
  Source(Environment* env) : env_(env) {}
  virtual ~Source() {}

  virtual int Pull(io_pull_t* handle,
                   size_t* length = 0,
                   io_buf_t* bufs = nullptr,
                   size_t count = 0,
                   int flags = IO_PULL_FLAG_NONE) = 0;

 private:
  Environment* env() const {
    return env_;
  }

  Environment* env_;
};

class Sink {
 public:
  Sink(Environment* env) : env_(env) {}
  virtual ~Sink() {}

  virtual void Signal(io_bind_t* handle,
                      io_signal signal = IO_SIGNAL_START,
                      size_t hint = 0) = 0;

  virtual void Bind(io_bind_t* handle,
                    Source* source,
                    int flags = IO_BIND_FLAG_NONE) = 0;

  virtual void Unbind(io_bind_t* handle) = 0;

  // Check the status of the bind operation
  virtual int Check(io_bind_t* handle) = 0;

 private:
  Environment* env() const {
    return env_;
  }

  Environment* env_;
};

}  // namespace io
}  // namespace node

#endif  // SRC_NODE_IO_H_
