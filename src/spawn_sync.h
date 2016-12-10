#ifndef SRC_SPAWN_SYNC_H_
#define SRC_SPAWN_SYNC_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_buffer.h"

namespace node {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;


class SyncProcessOutputBuffer;
class SyncProcessStdioPipe;
class SyncProcessRunner;


class SyncProcessOutputBuffer {
  static const unsigned int kBufferSize = 65536;

 public:
  inline SyncProcessOutputBuffer();

  inline void OnAlloc(size_t suggested_size, uv_buf_t* buf) const;
  inline void OnRead(const uv_buf_t* buf, size_t nread);

  inline size_t Copy(char* dest) const;

  inline unsigned int available() const;
  inline unsigned int used() const;

  inline SyncProcessOutputBuffer* next() const;
  inline void set_next(SyncProcessOutputBuffer* next);

 private:
  // Use unsigned int because that's what `uv_buf_init` takes.
  mutable char data_[kBufferSize];
  unsigned int used_;

  SyncProcessOutputBuffer* next_;
};


class SyncProcessStdioPipe {
  enum Lifecycle {
    kUninitialized = 0,
    kInitialized,
    kStarted,
    kClosing,
    kClosed
  };

 public:
  SyncProcessStdioPipe(SyncProcessRunner* process_handler,
                       bool readable,
                       bool writable,
                       uv_buf_t input_buffer);
  ~SyncProcessStdioPipe();

  int Initialize(uv_loop_t* loop);
  int Start();
  void Close();

  Local<Object> GetOutputAsBuffer(Environment* env) const;

  inline bool readable() const;
  inline bool writable() const;
  inline uv_stdio_flags uv_flags() const;

  inline uv_pipe_t* uv_pipe() const;
  inline uv_stream_t* uv_stream() const;
  inline uv_handle_t* uv_handle() const;

 private:
  inline size_t OutputLength() const;
  inline void CopyOutput(char* dest) const;

  inline void OnAlloc(size_t suggested_size, uv_buf_t* buf);
  inline void OnRead(const uv_buf_t* buf, ssize_t nread);
  inline void OnWriteDone(int result);
  inline void OnShutdownDone(int result);
  inline void OnClose();

  inline void SetError(int error);

  static void AllocCallback(uv_handle_t* handle,
                            size_t suggested_size,
                            uv_buf_t* buf);
  static void ReadCallback(uv_stream_t* stream,
                           ssize_t nread,
                           const uv_buf_t* buf);
  static void WriteCallback(uv_write_t* req, int result);
  static void ShutdownCallback(uv_shutdown_t* req, int result);
  static void CloseCallback(uv_handle_t* handle);

  SyncProcessRunner* process_handler_;

  bool readable_;
  bool writable_;
  uv_buf_t input_buffer_;

  SyncProcessOutputBuffer* first_output_buffer_;
  SyncProcessOutputBuffer* last_output_buffer_;

  mutable uv_pipe_t uv_pipe_;
  uv_write_t write_req_;
  uv_shutdown_t shutdown_req_;

  Lifecycle lifecycle_;
};


class SyncProcessRunner {
  enum Lifecycle {
    kUninitialized = 0,
    kInitialized,
    kHandlesClosed
  };

 public:
  static void Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context);
  static void Spawn(const FunctionCallbackInfo<Value>& args);

 private:
  friend class SyncProcessStdioPipe;

  explicit SyncProcessRunner(Environment* env_);
  ~SyncProcessRunner();

  inline Environment* env() const;

  Local<Object> Run(Local<Value> options);
  void TryInitializeAndRunLoop(Local<Value> options);
  void CloseHandlesAndDeleteLoop();

  void CloseStdioPipes();
  void CloseKillTimer();

  void Kill();
  void IncrementBufferSizeAndCheckOverflow(ssize_t length);

  void OnExit(int64_t exit_status, int term_signal);
  void OnKillTimerTimeout();

  int GetError();
  void SetError(int error);
  void SetPipeError(int pipe_error);

  Local<Object> BuildResultObject();
  Local<Array> BuildOutputArray();

  int ParseOptions(Local<Value> js_value);
  int ParseStdioOptions(Local<Value> js_value);
  int ParseStdioOption(int child_fd, Local<Object> js_stdio_option);

  inline int AddStdioIgnore(uint32_t child_fd);
  inline int AddStdioPipe(uint32_t child_fd,
                          bool readable,
                          bool writable,
                          uv_buf_t input_buffer);
  inline int AddStdioInheritFD(uint32_t child_fd, int inherit_fd);

  static bool IsSet(Local<Value> value);
  int CopyJsString(Local<Value> js_value, const char** target);
  int CopyJsStringArray(Local<Value> js_value, char** target);

  static void ExitCallback(uv_process_t* handle,
                           int64_t exit_status,
                           int term_signal);
  static void KillTimerCallback(uv_timer_t* handle);
  static void KillTimerCloseCallback(uv_handle_t* handle);

  size_t max_buffer_;
  uint64_t timeout_;
  int kill_signal_;

  uv_loop_t* uv_loop_;

  uint32_t stdio_count_;
  uv_stdio_container_t* uv_stdio_containers_;
  SyncProcessStdioPipe** stdio_pipes_;
  bool stdio_pipes_initialized_;

  uv_process_options_t uv_process_options_;
  const char* file_buffer_;
  char* args_buffer_;
  char* env_buffer_;
  const char* cwd_buffer_;

  uv_process_t uv_process_;
  bool killed_;

  size_t buffered_output_size_;
  int64_t exit_status_;
  int term_signal_;

  uv_timer_t uv_timer_;
  bool kill_timer_initialized_;

  // Errors that happen in one of the pipe handlers are stored in the
  // `pipe_error` field. They are treated as "low-priority", only to be
  // reported if no more serious errors happened.
  int error_;
  int pipe_error_;

  Lifecycle lifecycle_;

  Environment* env_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_SPAWN_SYNC_H_
