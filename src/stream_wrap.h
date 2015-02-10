#ifndef SRC_STREAM_WRAP_H_
#define SRC_STREAM_WRAP_H_

#include "env.h"
#include "handle_wrap.h"
#include "req-wrap.h"
#include "req-wrap-inl.h"
#include "stream_base.h"
#include "string_bytes.h"
#include "v8.h"

namespace node {

// Forward declaration
class StreamWrap;

class ShutdownWrap : public ReqWrap<uv_shutdown_t> {
 public:
  ShutdownWrap(Environment* env, v8::Local<v8::Object> req_wrap_obj)
      : ReqWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_SHUTDOWNWRAP) {
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
      : ReqWrap(env, obj, AsyncWrap::PROVIDER_WRITEWRAP),
        wrap_(wrap) {
    Wrap(obj, this);
  }

  void* operator new(size_t size, char* storage) { return storage; }

  // This is just to keep the compiler happy. It should never be called, since
  // we don't use exceptions in node.
  void operator delete(void* ptr, char* storage) { UNREACHABLE(); }

  inline StreamWrap* wrap() const {
    return wrap_;
  }

  static void NewWriteWrap(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK(args.IsConstructCall());
  }

 private:
  // People should not be using the non-placement new and delete operator on a
  // WriteWrap. Ensure this never happens.
  void* operator new(size_t size) { UNREACHABLE(); }
  void operator delete(void* ptr) { UNREACHABLE(); }

  StreamWrap* const wrap_;
};

// Overridable callbacks' types
class StreamWrapCallbacks {
 public:
  explicit StreamWrapCallbacks(StreamWrap* wrap) : wrap_(wrap) {
  }

  explicit StreamWrapCallbacks(StreamWrapCallbacks* old) : wrap_(old->wrap()) {
  }

  virtual ~StreamWrapCallbacks() = default;

  virtual const char* Error() const;
  virtual void ClearError();

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

class StreamWrap : public HandleWrap, public StreamBase {
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

  int GetFD();
  bool IsAlive();

  // JavaScript functions
  int ReadStart();
  int ReadStop();
  int Shutdown(v8::Local<v8::Object> req);
  int Writev(v8::Local<v8::Object> req, v8::Local<v8::Array> bufs);
  int WriteBuffer(v8::Local<v8::Object> req,
                  const char* buf,
                  size_t len);
  int WriteString(v8::Local<v8::Object> req,
                  v8::Local<v8::String> str,
                  enum encoding enc,
                  v8::Local<v8::Object> handle);
  int SetBlocking(bool enable);

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
  StreamWrap(Environment* env,
             v8::Local<v8::Object> object,
             uv_stream_t* stream,
             AsyncWrap::ProviderType provider,
             AsyncWrap* parent = nullptr);

  ~StreamWrap() {
    if (!callbacks_gc_ && callbacks_ != &default_callbacks_) {
      delete callbacks_;
    }
    callbacks_ = nullptr;
  }

  void StateChange() { }
  void UpdateWriteQueueSize();

 private:
  // Callbacks for libuv
  static void AfterWrite(uv_write_t* req, int status);
  static void OnAlloc(uv_handle_t* handle,
                      size_t suggested_size,
                      uv_buf_t* buf);

  static void OnRead(uv_stream_t* handle,
                     ssize_t nread,
                     const uv_buf_t* buf);
  static void OnReadCommon(uv_stream_t* handle,
                           ssize_t nread,
                           const uv_buf_t* buf,
                           uv_handle_type pending);
  static void AfterShutdown(uv_shutdown_t* req, int status);

  template <enum encoding encoding>
  int WriteStringImpl(v8::Local<v8::Object> req,
                      v8::Local<v8::String> str,
                      v8::Local<v8::Object> handle);

  uv_stream_t* const stream_;
  StreamWrapCallbacks default_callbacks_;
  StreamWrapCallbacks* callbacks_;  // Overridable callbacks
  bool callbacks_gc_;

  friend class StreamWrapCallbacks;
};


}  // namespace node


#endif  // SRC_STREAM_WRAP_H_
