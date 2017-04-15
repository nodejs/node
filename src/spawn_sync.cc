// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "spawn_sync.h"
#include "env-inl.h"
#include "string_bytes.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>


namespace node {

using v8::Array;
using v8::Context;
using v8::EscapableHandleScope;
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


SyncProcessOutputBuffer::SyncProcessOutputBuffer()
    : used_(0),
      next_(nullptr) {
}


void SyncProcessOutputBuffer::OnAlloc(size_t suggested_size,
                                      uv_buf_t* buf) const {
  if (used() == kBufferSize)
    *buf = uv_buf_init(nullptr, 0);
  else
    *buf = uv_buf_init(data_ + used(), available());
}


void SyncProcessOutputBuffer::OnRead(const uv_buf_t* buf, size_t nread) {
  // If we hand out the same chunk twice, this should catch it.
  CHECK_EQ(buf->base, data_ + used());
  used_ += static_cast<unsigned int>(nread);
}


size_t SyncProcessOutputBuffer::Copy(char* dest) const {
  memcpy(dest, data_, used());
  return used();
}


unsigned int SyncProcessOutputBuffer::available() const {
  return sizeof data_ - used();
}


unsigned int SyncProcessOutputBuffer::used() const {
  return used_;
}


SyncProcessOutputBuffer* SyncProcessOutputBuffer::next() const {
  return next_;
}


void SyncProcessOutputBuffer::set_next(SyncProcessOutputBuffer* next) {
  next_ = next;
}


SyncProcessStdioPipe::SyncProcessStdioPipe(SyncProcessRunner* process_handler,
                                           bool readable,
                                           bool writable,
                                           uv_buf_t input_buffer)
    : process_handler_(process_handler),
      readable_(readable),
      writable_(writable),
      input_buffer_(input_buffer),

      first_output_buffer_(nullptr),
      last_output_buffer_(nullptr),

      uv_pipe_(),
      write_req_(),
      shutdown_req_(),

      lifecycle_(kUninitialized) {
  CHECK(readable || writable);
}


SyncProcessStdioPipe::~SyncProcessStdioPipe() {
  CHECK(lifecycle_ == kUninitialized || lifecycle_ == kClosed);

  SyncProcessOutputBuffer* buf;
  SyncProcessOutputBuffer* next;

  for (buf = first_output_buffer_; buf != nullptr; buf = next) {
    next = buf->next();
    delete buf;
  }
}


int SyncProcessStdioPipe::Initialize(uv_loop_t* loop) {
  CHECK_EQ(lifecycle_, kUninitialized);

  int r = uv_pipe_init(loop, uv_pipe(), 0);
  if (r < 0)
    return r;

  uv_pipe()->data = this;

  lifecycle_ = kInitialized;
  return 0;
}


int SyncProcessStdioPipe::Start() {
  CHECK_EQ(lifecycle_, kInitialized);

  // Set the busy flag already. If this function fails no recovery is
  // possible.
  lifecycle_ = kStarted;

  if (readable()) {
    if (input_buffer_.len > 0) {
      CHECK_NE(input_buffer_.base, nullptr);

      int r = uv_write(&write_req_,
                       uv_stream(),
                       &input_buffer_,
                       1,
                       WriteCallback);
      if (r < 0)
        return r;
    }

    int r = uv_shutdown(&shutdown_req_, uv_stream(), ShutdownCallback);
    if (r < 0)
      return r;
  }

  if (writable()) {
    int r = uv_read_start(uv_stream(), AllocCallback, ReadCallback);
    if (r < 0)
      return r;
  }

  return 0;
}


void SyncProcessStdioPipe::Close() {
  CHECK(lifecycle_ == kInitialized || lifecycle_ == kStarted);

  uv_close(uv_handle(), CloseCallback);

  lifecycle_ = kClosing;
}


Local<Object> SyncProcessStdioPipe::GetOutputAsBuffer(Environment* env) const {
  size_t length = OutputLength();
  Local<Object> js_buffer = Buffer::New(env, length).ToLocalChecked();
  CopyOutput(Buffer::Data(js_buffer));
  return js_buffer;
}


bool SyncProcessStdioPipe::readable() const {
  return readable_;
}


bool SyncProcessStdioPipe::writable() const {
  return writable_;
}


uv_stdio_flags SyncProcessStdioPipe::uv_flags() const {
  unsigned int flags;

  flags = UV_CREATE_PIPE;
  if (readable())
    flags |= UV_READABLE_PIPE;
  if (writable())
    flags |= UV_WRITABLE_PIPE;

  return static_cast<uv_stdio_flags>(flags);
}


uv_pipe_t* SyncProcessStdioPipe::uv_pipe() const {
  CHECK_LT(lifecycle_, kClosing);
  return &uv_pipe_;
}


uv_stream_t* SyncProcessStdioPipe::uv_stream() const {
  return reinterpret_cast<uv_stream_t*>(uv_pipe());
}


uv_handle_t* SyncProcessStdioPipe::uv_handle() const {
  return reinterpret_cast<uv_handle_t*>(uv_pipe());
}


size_t SyncProcessStdioPipe::OutputLength() const {
  SyncProcessOutputBuffer* buf;
  size_t size = 0;

  for (buf = first_output_buffer_; buf != nullptr; buf = buf->next())
    size += buf->used();

  return size;
}


void SyncProcessStdioPipe::CopyOutput(char* dest) const {
  SyncProcessOutputBuffer* buf;
  size_t offset = 0;

  for (buf = first_output_buffer_; buf != nullptr; buf = buf->next())
    offset += buf->Copy(dest + offset);
}


void SyncProcessStdioPipe::OnAlloc(size_t suggested_size, uv_buf_t* buf) {
  // This function assumes that libuv will never allocate two buffers for the
  // same stream at the same time. There's an assert in
  // SyncProcessOutputBuffer::OnRead that would fail if this assumption was
  // ever violated.

  if (last_output_buffer_ == nullptr) {
    // Allocate the first capture buffer.
    first_output_buffer_ = new SyncProcessOutputBuffer();
    last_output_buffer_ = first_output_buffer_;

  } else if (last_output_buffer_->available() == 0) {
    // The current capture buffer is full so get us a new one.
    SyncProcessOutputBuffer* buf = new SyncProcessOutputBuffer();
    last_output_buffer_->set_next(buf);
    last_output_buffer_ = buf;
  }

  last_output_buffer_->OnAlloc(suggested_size, buf);
}


void SyncProcessStdioPipe::OnRead(const uv_buf_t* buf, ssize_t nread) {
  if (nread == UV_EOF) {
    // Libuv implicitly stops reading on EOF.

  } else if (nread < 0) {
    SetError(static_cast<int>(nread));
    // At some point libuv should really implicitly stop reading on error.
    uv_read_stop(uv_stream());

  } else {
    last_output_buffer_->OnRead(buf, nread);
    process_handler_->IncrementBufferSizeAndCheckOverflow(nread);
  }
}


void SyncProcessStdioPipe::OnWriteDone(int result) {
  if (result < 0)
    SetError(result);
}


void SyncProcessStdioPipe::OnShutdownDone(int result) {
  if (result < 0)
    SetError(result);
}


void SyncProcessStdioPipe::OnClose() {
  lifecycle_ = kClosed;
}


void SyncProcessStdioPipe::SetError(int error) {
  CHECK_NE(error, 0);
  process_handler_->SetPipeError(error);
}


void SyncProcessStdioPipe::AllocCallback(uv_handle_t* handle,
                                         size_t suggested_size,
                                         uv_buf_t* buf) {
  SyncProcessStdioPipe* self =
      reinterpret_cast<SyncProcessStdioPipe*>(handle->data);
  self->OnAlloc(suggested_size, buf);
}


void SyncProcessStdioPipe::ReadCallback(uv_stream_t* stream,
                                        ssize_t nread,
                                        const uv_buf_t* buf) {
  SyncProcessStdioPipe* self =
        reinterpret_cast<SyncProcessStdioPipe*>(stream->data);
  self->OnRead(buf, nread);
}


void SyncProcessStdioPipe::WriteCallback(uv_write_t* req, int result) {
  SyncProcessStdioPipe* self =
      reinterpret_cast<SyncProcessStdioPipe*>(req->handle->data);
  self->OnWriteDone(result);
}


void SyncProcessStdioPipe::ShutdownCallback(uv_shutdown_t* req, int result) {
  SyncProcessStdioPipe* self =
      reinterpret_cast<SyncProcessStdioPipe*>(req->handle->data);

  // On AIX, OS X and the BSDs, calling shutdown() on one end of a pipe
  // when the other end has closed the connection fails with ENOTCONN.
  // Libuv is not the right place to handle that because it can't tell
  // if the error is genuine but we here can.
  if (result == UV_ENOTCONN)
    result = 0;

  self->OnShutdownDone(result);
}


void SyncProcessStdioPipe::CloseCallback(uv_handle_t* handle) {
  SyncProcessStdioPipe* self =
      reinterpret_cast<SyncProcessStdioPipe*>(handle->data);
  self->OnClose();
}


void SyncProcessRunner::Initialize(Local<Object> target,
                                   Local<Value> unused,
                                   Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "spawn", Spawn);
}


void SyncProcessRunner::Spawn(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  env->PrintSyncTrace();
  SyncProcessRunner p(env);
  Local<Value> result = p.Run(args[0]);
  args.GetReturnValue().Set(result);
}


SyncProcessRunner::SyncProcessRunner(Environment* env)
    : max_buffer_(0),
      timeout_(0),
      kill_signal_(SIGTERM),

      uv_loop_(nullptr),

      stdio_count_(0),
      uv_stdio_containers_(nullptr),
      stdio_pipes_(nullptr),
      stdio_pipes_initialized_(false),

      uv_process_options_(),
      file_buffer_(nullptr),
      args_buffer_(nullptr),
      env_buffer_(nullptr),
      cwd_buffer_(nullptr),

      uv_process_(),
      killed_(false),

      buffered_output_size_(0),
      exit_status_(-1),
      term_signal_(-1),

      uv_timer_(),
      kill_timer_initialized_(false),

      error_(0),
      pipe_error_(0),

      lifecycle_(kUninitialized),

      env_(env) {
}


SyncProcessRunner::~SyncProcessRunner() {
  CHECK_EQ(lifecycle_, kHandlesClosed);

  if (stdio_pipes_ != nullptr) {
    for (size_t i = 0; i < stdio_count_; i++) {
      if (stdio_pipes_[i] != nullptr)
        delete stdio_pipes_[i];
    }
  }

  delete[] stdio_pipes_;
  delete[] file_buffer_;
  delete[] args_buffer_;
  delete[] cwd_buffer_;
  delete[] env_buffer_;
  delete[] uv_stdio_containers_;
}


Environment* SyncProcessRunner::env() const {
  return env_;
}


Local<Object> SyncProcessRunner::Run(Local<Value> options) {
  EscapableHandleScope scope(env()->isolate());

  CHECK_EQ(lifecycle_, kUninitialized);

  TryInitializeAndRunLoop(options);
  CloseHandlesAndDeleteLoop();

  Local<Object> result = BuildResultObject();

  return scope.Escape(result);
}


void SyncProcessRunner::TryInitializeAndRunLoop(Local<Value> options) {
  int r;

  // There is no recovery from failure inside TryInitializeAndRunLoop - the
  // only option we'd have is to close all handles and destroy the loop.
  CHECK_EQ(lifecycle_, kUninitialized);
  lifecycle_ = kInitialized;

  uv_loop_ = new uv_loop_t;
  if (uv_loop_ == nullptr)
    return SetError(UV_ENOMEM);
  CHECK_EQ(uv_loop_init(uv_loop_), 0);

  r = ParseOptions(options);
  if (r < 0)
    return SetError(r);

  if (timeout_ > 0) {
    r = uv_timer_init(uv_loop_, &uv_timer_);
    if (r < 0)
      return SetError(r);

    uv_unref(reinterpret_cast<uv_handle_t*>(&uv_timer_));

    uv_timer_.data = this;
    kill_timer_initialized_ = true;

    // Start the timer immediately. If uv_spawn fails then
    // CloseHandlesAndDeleteLoop() will immediately close the timer handle
    // which implicitly stops it, so there is no risk that the timeout callback
    // runs when the process didn't start.
    r = uv_timer_start(&uv_timer_, KillTimerCallback, timeout_, 0);
    if (r < 0)
      return SetError(r);
  }

  uv_process_options_.exit_cb = ExitCallback;
  r = uv_spawn(uv_loop_, &uv_process_, &uv_process_options_);
  if (r < 0)
    return SetError(r);
  uv_process_.data = this;

  for (uint32_t i = 0; i < stdio_count_; i++) {
    SyncProcessStdioPipe* h = stdio_pipes_[i];
    if (h != nullptr) {
      r = h->Start();
      if (r < 0)
        return SetPipeError(r);
    }
  }

  r = uv_run(uv_loop_, UV_RUN_DEFAULT);
  if (r < 0)
    // We can't handle uv_run failure.
    ABORT();

  // If we get here the process should have exited.
  CHECK_GE(exit_status_, 0);
}


void SyncProcessRunner::CloseHandlesAndDeleteLoop() {
  CHECK_LT(lifecycle_, kHandlesClosed);

  if (uv_loop_ != nullptr) {
    CloseStdioPipes();
    CloseKillTimer();
    // Close the process handle when ExitCallback was not called.
    uv_handle_t* uv_process_handle =
        reinterpret_cast<uv_handle_t*>(&uv_process_);

    // Close the process handle if it is still open. The handle type also
    // needs to be checked because TryInitializeAndRunLoop() won't spawn a
    // process if input validation fails.
    if (uv_process_handle->type == UV_PROCESS &&
        !uv_is_closing(uv_process_handle))
      uv_close(uv_process_handle, nullptr);

    // Give closing watchers a chance to finish closing and get their close
    // callbacks called.
    int r = uv_run(uv_loop_, UV_RUN_DEFAULT);
    if (r < 0)
      ABORT();

    CHECK_EQ(uv_loop_close(uv_loop_), 0);
    delete uv_loop_;
    uv_loop_ = nullptr;

  } else {
    // If the loop doesn't exist, neither should any pipes or timers.
    CHECK_EQ(false, stdio_pipes_initialized_);
    CHECK_EQ(false, kill_timer_initialized_);
  }

  lifecycle_ = kHandlesClosed;
}


void SyncProcessRunner::CloseStdioPipes() {
  CHECK_LT(lifecycle_, kHandlesClosed);

  if (stdio_pipes_initialized_) {
    CHECK_NE(stdio_pipes_, nullptr);
    CHECK_NE(uv_loop_, nullptr);

    for (uint32_t i = 0; i < stdio_count_; i++) {
      if (stdio_pipes_[i] != nullptr)
        stdio_pipes_[i]->Close();
    }

    stdio_pipes_initialized_ = false;
  }
}


void SyncProcessRunner::CloseKillTimer() {
  CHECK_LT(lifecycle_, kHandlesClosed);

  if (kill_timer_initialized_) {
    CHECK_GT(timeout_, 0);
    CHECK_NE(uv_loop_, nullptr);

    uv_handle_t* uv_timer_handle = reinterpret_cast<uv_handle_t*>(&uv_timer_);
    uv_ref(uv_timer_handle);
    uv_close(uv_timer_handle, KillTimerCloseCallback);

    kill_timer_initialized_ = false;
  }
}


void SyncProcessRunner::Kill() {
  // Only attempt to kill once.
  if (killed_)
    return;
  killed_ = true;

  // We might get here even if the process we spawned has already exited. This
  // could happen when our child process spawned another process which
  // inherited (one of) the stdio pipes. In this case we won't attempt to send
  // a signal to the process, however we will still close our end of the stdio
  // pipes so this situation won't make us hang.
  if (exit_status_ < 0) {
    int r = uv_process_kill(&uv_process_, kill_signal_);

    // If uv_kill failed with an error that isn't ESRCH, the user probably
    // specified an invalid or unsupported signal. Signal this to the user as
    // and error and kill the process with SIGKILL instead.
    if (r < 0 && r != UV_ESRCH) {
      SetError(r);

      r = uv_process_kill(&uv_process_, SIGKILL);
      CHECK(r >= 0 || r == UV_ESRCH);
    }
  }

  // Close all stdio pipes.
  CloseStdioPipes();

  // Stop the timeout timer immediately.
  CloseKillTimer();
}


void SyncProcessRunner::IncrementBufferSizeAndCheckOverflow(ssize_t length) {
  buffered_output_size_ += length;

  if (max_buffer_ > 0 && buffered_output_size_ > max_buffer_) {
    SetError(UV_ENOBUFS);
    Kill();
  }
}


void SyncProcessRunner::OnExit(int64_t exit_status, int term_signal) {
  if (exit_status < 0)
    return SetError(static_cast<int>(exit_status));

  exit_status_ = exit_status;
  term_signal_ = term_signal;
}


void SyncProcessRunner::OnKillTimerTimeout() {
  SetError(UV_ETIMEDOUT);
  Kill();
}


int SyncProcessRunner::GetError() {
  if (error_ != 0)
    return error_;
  else
    return pipe_error_;
}


void SyncProcessRunner::SetError(int error) {
  if (error_ == 0)
    error_ = error;
}


void SyncProcessRunner::SetPipeError(int pipe_error) {
  if (pipe_error_ == 0)
    pipe_error_ = pipe_error;
}


Local<Object> SyncProcessRunner::BuildResultObject() {
  EscapableHandleScope scope(env()->isolate());

  Local<Object> js_result = Object::New(env()->isolate());

  if (GetError() != 0) {
    js_result->Set(env()->error_string(),
                   Integer::New(env()->isolate(), GetError()));
  }

  if (exit_status_ >= 0) {
    if (term_signal_ > 0) {
      js_result->Set(env()->status_string(), Null(env()->isolate()));
    } else {
      js_result->Set(env()->status_string(),
          Number::New(env()->isolate(), static_cast<double>(exit_status_)));
    }
  } else {
    // If exit_status_ < 0 the process was never started because of some error.
    js_result->Set(env()->status_string(), Null(env()->isolate()));
  }

  if (term_signal_ > 0)
    js_result->Set(env()->signal_string(),
        String::NewFromUtf8(env()->isolate(), signo_string(term_signal_)));
  else
    js_result->Set(env()->signal_string(), Null(env()->isolate()));

  if (exit_status_ >= 0)
    js_result->Set(env()->output_string(), BuildOutputArray());
  else
    js_result->Set(env()->output_string(), Null(env()->isolate()));

  js_result->Set(env()->pid_string(),
                 Number::New(env()->isolate(), uv_process_.pid));

  return scope.Escape(js_result);
}


Local<Array> SyncProcessRunner::BuildOutputArray() {
  CHECK_GE(lifecycle_, kInitialized);
  CHECK_NE(stdio_pipes_, nullptr);

  EscapableHandleScope scope(env()->isolate());
  Local<Array> js_output = Array::New(env()->isolate(), stdio_count_);

  for (uint32_t i = 0; i < stdio_count_; i++) {
    SyncProcessStdioPipe* h = stdio_pipes_[i];
    if (h != nullptr && h->writable())
      js_output->Set(i, h->GetOutputAsBuffer(env()));
    else
      js_output->Set(i, Null(env()->isolate()));
  }

  return scope.Escape(js_output);
}


int SyncProcessRunner::ParseOptions(Local<Value> js_value) {
  HandleScope scope(env()->isolate());
  int r;

  if (!js_value->IsObject())
    return UV_EINVAL;

  Local<Object> js_options = js_value.As<Object>();

  Local<Value> js_file = js_options->Get(env()->file_string());
  r = CopyJsString(js_file, &file_buffer_);
  if (r < 0)
    return r;
  uv_process_options_.file = file_buffer_;

  Local<Value> js_args = js_options->Get(env()->args_string());
  r = CopyJsStringArray(js_args, &args_buffer_);
  if (r < 0)
    return r;
  uv_process_options_.args = reinterpret_cast<char**>(args_buffer_);


  Local<Value> js_cwd = js_options->Get(env()->cwd_string());
  if (IsSet(js_cwd)) {
    r = CopyJsString(js_cwd, &cwd_buffer_);
    if (r < 0)
      return r;
    uv_process_options_.cwd = cwd_buffer_;
  }

  Local<Value> js_env_pairs = js_options->Get(env()->env_pairs_string());
  if (IsSet(js_env_pairs)) {
    r = CopyJsStringArray(js_env_pairs, &env_buffer_);
    if (r < 0)
      return r;

    uv_process_options_.env = reinterpret_cast<char**>(env_buffer_);
  }
  Local<Value> js_uid = js_options->Get(env()->uid_string());
  if (IsSet(js_uid)) {
    CHECK(js_uid->IsInt32());
    const int32_t uid = js_uid->Int32Value(env()->context()).FromJust();
    uv_process_options_.uid = static_cast<uv_uid_t>(uid);
    uv_process_options_.flags |= UV_PROCESS_SETUID;
  }

  Local<Value> js_gid = js_options->Get(env()->gid_string());
  if (IsSet(js_gid)) {
    CHECK(js_gid->IsInt32());
    const int32_t gid = js_gid->Int32Value(env()->context()).FromJust();
    uv_process_options_.gid = static_cast<uv_gid_t>(gid);
    uv_process_options_.flags |= UV_PROCESS_SETGID;
  }

  if (js_options->Get(env()->detached_string())->BooleanValue())
    uv_process_options_.flags |= UV_PROCESS_DETACHED;

  Local<String> wba = env()->windows_verbatim_arguments_string();

  if (js_options->Get(wba)->BooleanValue())
    uv_process_options_.flags |= UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS;

  Local<Value> js_timeout = js_options->Get(env()->timeout_string());
  if (IsSet(js_timeout)) {
    CHECK(js_timeout->IsNumber());
    int64_t timeout = js_timeout->IntegerValue();
    timeout_ = static_cast<uint64_t>(timeout);
  }

  Local<Value> js_max_buffer = js_options->Get(env()->max_buffer_string());
  if (IsSet(js_max_buffer)) {
    CHECK(js_max_buffer->IsNumber());
    max_buffer_ = js_max_buffer->NumberValue();
  }

  Local<Value> js_kill_signal = js_options->Get(env()->kill_signal_string());
  if (IsSet(js_kill_signal)) {
    CHECK(js_kill_signal->IsInt32());
    kill_signal_ = js_kill_signal->Int32Value();
  }

  Local<Value> js_stdio = js_options->Get(env()->stdio_string());
  r = ParseStdioOptions(js_stdio);
  if (r < 0)
    return r;

  return 0;
}


int SyncProcessRunner::ParseStdioOptions(Local<Value> js_value) {
  HandleScope scope(env()->isolate());
  Local<Array> js_stdio_options;

  if (!js_value->IsArray())
    return UV_EINVAL;

  js_stdio_options = js_value.As<Array>();

  stdio_count_ = js_stdio_options->Length();
  uv_stdio_containers_ = new uv_stdio_container_t[stdio_count_];

  stdio_pipes_ = new SyncProcessStdioPipe*[stdio_count_]();
  stdio_pipes_initialized_ = true;

  for (uint32_t i = 0; i < stdio_count_; i++) {
    Local<Value> js_stdio_option = js_stdio_options->Get(i);

    if (!js_stdio_option->IsObject())
      return UV_EINVAL;

    int r = ParseStdioOption(i, js_stdio_option.As<Object>());
    if (r < 0)
      return r;
  }

  uv_process_options_.stdio = uv_stdio_containers_;
  uv_process_options_.stdio_count = stdio_count_;

  return 0;
}


int SyncProcessRunner::ParseStdioOption(int child_fd,
                                        Local<Object> js_stdio_option) {
  Local<Value> js_type = js_stdio_option->Get(env()->type_string());

  if (js_type->StrictEquals(env()->ignore_string())) {
    return AddStdioIgnore(child_fd);

  } else if (js_type->StrictEquals(env()->pipe_string())) {
    Local<String> rs = env()->readable_string();
    Local<String> ws = env()->writable_string();

    bool readable = js_stdio_option->Get(rs)->BooleanValue();
    bool writable = js_stdio_option->Get(ws)->BooleanValue();

    uv_buf_t buf = uv_buf_init(nullptr, 0);

    if (readable) {
      Local<Value> input = js_stdio_option->Get(env()->input_string());
      if (Buffer::HasInstance(input)) {
        buf = uv_buf_init(Buffer::Data(input),
                          static_cast<unsigned int>(Buffer::Length(input)));
      } else if (!input->IsUndefined() && !input->IsNull()) {
        // Strings, numbers etc. are currently unsupported. It's not possible
        // to create a buffer for them here because there is no way to free
        // them afterwards.
        return UV_EINVAL;
      }
    }

    return AddStdioPipe(child_fd, readable, writable, buf);

  } else if (js_type->StrictEquals(env()->inherit_string()) ||
             js_type->StrictEquals(env()->fd_string())) {
    int inherit_fd = js_stdio_option->Get(env()->fd_string())->Int32Value();
    return AddStdioInheritFD(child_fd, inherit_fd);

  } else {
    CHECK(0 && "invalid child stdio type");
    return UV_EINVAL;
  }
}


int SyncProcessRunner::AddStdioIgnore(uint32_t child_fd) {
  CHECK_LT(child_fd, stdio_count_);
  CHECK_EQ(stdio_pipes_[child_fd], nullptr);

  uv_stdio_containers_[child_fd].flags = UV_IGNORE;

  return 0;
}


int SyncProcessRunner::AddStdioPipe(uint32_t child_fd,
                                    bool readable,
                                    bool writable,
                                    uv_buf_t input_buffer) {
  CHECK_LT(child_fd, stdio_count_);
  CHECK_EQ(stdio_pipes_[child_fd], nullptr);

  SyncProcessStdioPipe* h = new SyncProcessStdioPipe(this,
                                                     readable,
                                                     writable,
                                                     input_buffer);

  int r = h->Initialize(uv_loop_);
  if (r < 0) {
    delete h;
    return r;
  }

  stdio_pipes_[child_fd] = h;

  uv_stdio_containers_[child_fd].flags = h->uv_flags();
  uv_stdio_containers_[child_fd].data.stream = h->uv_stream();

  return 0;
}


int SyncProcessRunner::AddStdioInheritFD(uint32_t child_fd, int inherit_fd) {
  CHECK_LT(child_fd, stdio_count_);
  CHECK_EQ(stdio_pipes_[child_fd], nullptr);

  uv_stdio_containers_[child_fd].flags = UV_INHERIT_FD;
  uv_stdio_containers_[child_fd].data.fd = inherit_fd;

  return 0;
}


bool SyncProcessRunner::IsSet(Local<Value> value) {
  return !value->IsUndefined() && !value->IsNull();
}


int SyncProcessRunner::CopyJsString(Local<Value> js_value,
                                    const char** target) {
  Isolate* isolate = env()->isolate();
  Local<String> js_string;
  size_t size, written;
  char* buffer;

  if (js_value->IsString())
    js_string = js_value.As<String>();
  else
    js_string = js_value->ToString(env()->isolate());

  // Include space for null terminator byte.
  size = StringBytes::StorageSize(isolate, js_string, UTF8) + 1;

  buffer = new char[size];

  written = StringBytes::Write(isolate, buffer, -1, js_string, UTF8);
  buffer[written] = '\0';

  *target = buffer;
  return 0;
}


int SyncProcessRunner::CopyJsStringArray(Local<Value> js_value,
                                         char** target) {
  Isolate* isolate = env()->isolate();
  Local<Array> js_array;
  uint32_t length;
  size_t list_size, data_size, data_offset;
  char** list;
  char* buffer;

  if (!js_value->IsArray())
    return UV_EINVAL;

  js_array = js_value.As<Array>()->Clone().As<Array>();
  length = js_array->Length();

  // Convert all array elements to string. Modify the js object itself if
  // needed - it's okay since we cloned the original object.
  for (uint32_t i = 0; i < length; i++) {
    if (!js_array->Get(i)->IsString())
      js_array->Set(i, js_array->Get(i)->ToString(env()->isolate()));
  }

  // Index has a pointer to every string element, plus one more for a final
  // null pointer.
  list_size = (length + 1) * sizeof *list;

  // Compute the length of all strings. Include room for null terminator
  // after every string. Align strings to cache lines.
  data_size = 0;
  for (uint32_t i = 0; i < length; i++) {
    data_size += StringBytes::StorageSize(isolate, js_array->Get(i), UTF8) + 1;
    data_size = ROUND_UP(data_size, sizeof(void*));
  }

  buffer = new char[list_size + data_size];

  list = reinterpret_cast<char**>(buffer);
  data_offset = list_size;

  for (uint32_t i = 0; i < length; i++) {
    list[i] = buffer + data_offset;
    data_offset += StringBytes::Write(isolate,
                                      buffer + data_offset,
                                      -1,
                                      js_array->Get(i),
                                      UTF8);
    buffer[data_offset++] = '\0';
    data_offset = ROUND_UP(data_offset, sizeof(void*));
  }

  list[length] = nullptr;

  *target = buffer;
  return 0;
}


void SyncProcessRunner::ExitCallback(uv_process_t* handle,
                                     int64_t exit_status,
                                     int term_signal) {
  SyncProcessRunner* self = reinterpret_cast<SyncProcessRunner*>(handle->data);
  uv_close(reinterpret_cast<uv_handle_t*>(handle), nullptr);
  self->OnExit(exit_status, term_signal);
}


void SyncProcessRunner::KillTimerCallback(uv_timer_t* handle) {
  SyncProcessRunner* self = reinterpret_cast<SyncProcessRunner*>(handle->data);
  self->OnKillTimerTimeout();
}


void SyncProcessRunner::KillTimerCloseCallback(uv_handle_t* handle) {
  // No-op.
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(spawn_sync,
  node::SyncProcessRunner::Initialize)
