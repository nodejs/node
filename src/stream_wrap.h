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

#ifndef SRC_STREAM_WRAP_H_
#define SRC_STREAM_WRAP_H_

#include "env.h"
#include "handle_wrap.h"
#include "req_wrap.h"
#include "string_bytes.h"
#include "v8.h"

namespace node {

// Forward declaration
class StreamWrap;

class ShutdownWrap : public ReqWrap<uv_shutdown_t> {
 public:
  ShutdownWrap(Environment* env, v8::Local<v8::Object> req_wrap_obj)
      : ReqWrap<uv_shutdown_t>(env,
                               req_wrap_obj,
                               AsyncWrap::PROVIDER_SHUTDOWNWRAP) {
    Wrap(req_wrap_obj, this);
  }

  static void NewShutdownWrap(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK(args.IsConstructCall());
  }
};

class WriteWrap: public ReqWrap<uv_write_t> {
 public:
  // TODO(trevnorris): WrapWrap inherits from ReqWrap, which I've globbed
  // into the same provider. How should these be broken apart?
  WriteWrap(Environment* env, v8::Local<v8::Object> obj, StreamWrap* wrap)
      : ReqWrap<uv_write_t>(env, obj, AsyncWrap::PROVIDER_WRITEWRAP),
        wrap_(wrap) {
    Wrap(obj, this);
  }

  void* operator new(size_t size, char* storage) { return storage; }

  // This is just to keep the compiler happy. It should never be called, since
  // we don't use exceptions in node.
  void operator delete(void* ptr, char* storage) { assert(0); }

  inline StreamWrap* wrap() const {
    return wrap_;
  }

  static void NewWriteWrap(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK(args.IsConstructCall());
  }

 private:
  // People should not be using the non-placement new and delete operator on a
  // WriteWrap. Ensure this never happens.
  void* operator new(size_t size) { assert(0); }
  void operator delete(void* ptr) { assert(0); }

  StreamWrap* const wrap_;
};

// Overridable callbacks' types
class StreamWrapCallbacks {
 public:
  explicit StreamWrapCallbacks(StreamWrap* wrap) : wrap_(wrap) {
  }

  explicit StreamWrapCallbacks(StreamWrapCallbacks* old) : wrap_(old->wrap()) {
  }

  virtual ~StreamWrapCallbacks() {
  }

  virtual const char* Error();

  virtual int TryWrite(uv_buf_t** bufs, size_t* count);

  virtual int DoWrite(WriteWrap* w,
                      uv_buf_t* bufs,
                      size_t count,
                      uv_stream_t* send_handle,
                      uv_write_cb cb);
  virtual void AfterWrite(WriteWrap* w);
  virtual void DoAlloc(uv_handle_t* handle,
                       size_t suggested_size,
                       uv_buf_t* buf);
  virtual void DoRead(uv_stream_t* handle,
                      ssize_t nread,
                      const uv_buf_t* buf,
                      uv_handle_type pending);
  virtual int DoShutdown(ShutdownWrap* req_wrap, uv_shutdown_cb cb);

 protected:
  inline StreamWrap* wrap() const {
    return wrap_;
  }

 private:
  StreamWrap* const wrap_;
};

class StreamWrap : public HandleWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target,
                         v8::Handle<v8::Value> unused,
                         v8::Handle<v8::Context> context);

  void OverrideCallbacks(StreamWrapCallbacks* callbacks, bool gc) {
    StreamWrapCallbacks* old = callbacks_;
    callbacks_ = callbacks;
    callbacks_gc_ = gc;
    if (old != &default_callbacks_)
      delete old;
  }

  static void GetFD(v8::Local<v8::String>,
                    const v8::PropertyCallbackInfo<v8::Value>&);

  // JavaScript functions
  static void ReadStart(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ReadStop(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Shutdown(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Writev(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WriteBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WriteAsciiString(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WriteUtf8String(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WriteUcs2String(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void WriteBinaryString(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void SetBlocking(const v8::FunctionCallbackInfo<v8::Value>& args);

  inline StreamWrapCallbacks* callbacks() const {
    return callbacks_;
  }

  inline uv_stream_t* stream() const {
    return stream_;
  }

  inline bool is_named_pipe() const {
    return stream()->type == UV_NAMED_PIPE;
  }

  inline bool is_named_pipe_ipc() const {
    return is_named_pipe() &&
           reinterpret_cast<const uv_pipe_t*>(stream())->ipc != 0;
  }

  inline bool is_tcp() const {
    return stream()->type == UV_TCP;
  }

 protected:
  static size_t WriteBuffer(v8::Handle<v8::Value> val, uv_buf_t* buf);

  StreamWrap(Environment* env,
             v8::Local<v8::Object> object,
             uv_stream_t* stream,
             AsyncWrap::ProviderType provider,
             AsyncWrap* parent = NULL);

  ~StreamWrap() {
    if (!callbacks_gc_ && callbacks_ != &default_callbacks_) {
      delete callbacks_;
    }
    callbacks_ = NULL;
  }

  void StateChange() { }
  void UpdateWriteQueueSize();

 private:
  // Callbacks for libuv
  static void AfterWrite(uv_write_t* req, int status);
  static void OnAlloc(uv_handle_t* handle,
                      size_t suggested_size,
                      uv_buf_t* buf);
  static void AfterShutdown(uv_shutdown_t* req, int status);

  static void OnRead(uv_stream_t* handle,
                     ssize_t nread,
                     const uv_buf_t* buf);
  static void OnReadCommon(uv_stream_t* handle,
                           ssize_t nread,
                           const uv_buf_t* buf,
                           uv_handle_type pending);

  template <enum encoding encoding>
  static void WriteStringImpl(const v8::FunctionCallbackInfo<v8::Value>& args);

  uv_stream_t* const stream_;
  StreamWrapCallbacks default_callbacks_;
  StreamWrapCallbacks* callbacks_;  // Overridable callbacks
  bool callbacks_gc_;

  friend class StreamWrapCallbacks;
};


}  // namespace node


#endif  // SRC_STREAM_WRAP_H_
