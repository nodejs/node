#ifndef SRC_STREAM_BASE_H_
#define SRC_STREAM_BASE_H_

#include "env.h"
#include "async-wrap.h"
#include "req-wrap.h"
#include "req-wrap-inl.h"
#include "node.h"

#include "v8.h"

namespace node {

// Forward declarations
class StreamBase;

template <class Req>
class StreamReq {
 public:
  typedef void (*DoneCb)(Req* req, int status);

  explicit StreamReq(DoneCb cb) : cb_(cb) {
  }

  inline void Done(int status) {
    cb_(static_cast<Req*>(this), status);
  }

 private:
  DoneCb cb_;
};

class ShutdownWrap : public ReqWrap<uv_shutdown_t>,
                     public StreamReq<ShutdownWrap> {
 public:
  ShutdownWrap(Environment* env,
               v8::Local<v8::Object> req_wrap_obj,
               StreamBase* wrap,
               DoneCb cb)
      : ReqWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_SHUTDOWNWRAP),
        StreamReq<ShutdownWrap>(cb),
        wrap_(wrap) {
    Wrap(req_wrap_obj, this);
  }

  static void NewShutdownWrap(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK(args.IsConstructCall());
  }

  inline StreamBase* wrap() const { return wrap_; }

 private:
  StreamBase* const wrap_;
};

class WriteWrap: public ReqWrap<uv_write_t>,
                 public StreamReq<WriteWrap> {
 public:
  static inline WriteWrap* New(Environment* env,
                               v8::Local<v8::Object> obj,
                               StreamBase* wrap,
                               DoneCb cb,
                               size_t extra = 0);
  inline void Dispose();
  inline char* Extra(size_t offset = 0);

  inline StreamBase* wrap() const { return wrap_; }

  static void NewWriteWrap(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK(args.IsConstructCall());
  }

  static const size_t kAlignSize = 16;

 protected:
  WriteWrap(Environment* env,
            v8::Local<v8::Object> obj,
            StreamBase* wrap,
            DoneCb cb)
      : ReqWrap(env, obj, AsyncWrap::PROVIDER_WRITEWRAP),
        StreamReq<WriteWrap>(cb),
        wrap_(wrap) {
    Wrap(obj, this);
  }

  void* operator new(size_t size) = delete;
  void* operator new(size_t size, char* storage) { return storage; }

  // This is just to keep the compiler happy. It should never be called, since
  // we don't use exceptions in node.
  void operator delete(void* ptr, char* storage) { UNREACHABLE(); }

 private:
  // People should not be using the non-placement new and delete operator on a
  // WriteWrap. Ensure this never happens.
  void operator delete(void* ptr) { UNREACHABLE(); }

  StreamBase* const wrap_;
};

class StreamResource {
 public:
  typedef void (*AfterWriteCb)(WriteWrap* w, void* ctx);
  typedef void (*AllocCb)(size_t size, uv_buf_t* buf, void* ctx);
  typedef void (*ReadCb)(ssize_t nread,
                         const uv_buf_t* buf,
                         uv_handle_type pending,
                         void* ctx);

  StreamResource() : after_write_cb_(nullptr),
                     alloc_cb_(nullptr),
                     read_cb_(nullptr) {
  }

  virtual ~StreamResource() = default;

  virtual int DoShutdown(ShutdownWrap* req_wrap) = 0;
  virtual int DoTryWrite(uv_buf_t** bufs, size_t* count);
  virtual int DoWrite(WriteWrap* w,
                      uv_buf_t* bufs,
                      size_t count,
                      uv_stream_t* send_handle) = 0;
  virtual const char* Error() const;
  virtual void ClearError();

  // Events
  inline void OnAfterWrite(WriteWrap* w) {
    if (after_write_cb_ != nullptr)
      after_write_cb_(w, after_write_ctx_);
  }

  inline void OnAlloc(size_t size, uv_buf_t* buf) {
    if (alloc_cb_ != nullptr)
      alloc_cb_(size, buf, alloc_ctx_);
  }

  inline void OnRead(size_t nread,
                     const uv_buf_t* buf,
                     uv_handle_type pending = UV_UNKNOWN_HANDLE) {
    if (read_cb_ != nullptr)
      read_cb_(nread, buf, pending, read_ctx_);
  }

  inline void set_after_write_cb(AfterWriteCb cb, void* ctx) {
    after_write_ctx_ = ctx;
    after_write_cb_ = cb;
  }

  inline void set_alloc_cb(AllocCb cb, void* ctx) {
    alloc_cb_ = cb;
    alloc_ctx_ = ctx;
  }

  inline void set_read_cb(ReadCb cb, void* ctx) {
    read_cb_ = cb;
    read_ctx_ = ctx;
  }

 private:
  AfterWriteCb after_write_cb_;
  void* after_write_ctx_;
  AllocCb alloc_cb_;
  void* alloc_ctx_;
  ReadCb read_cb_;
  void* read_ctx_;
};

class StreamBase : public StreamResource {
 public:
  enum Flags {
    kFlagNone = 0x0,
    kFlagHasWritev = 0x1,
    kFlagNoShutdown = 0x2
  };

  template <class Base>
  static inline void AddMethods(Environment* env,
                                v8::Handle<v8::FunctionTemplate> target,
                                int flags = kFlagNone);

  virtual void* Cast() = 0;
  virtual bool IsAlive() = 0;
  virtual bool IsClosing() = 0;
  virtual bool IsIPCPipe();
  virtual int GetFD();

  virtual int ReadStart() = 0;
  virtual int ReadStop() = 0;

  inline void Consume() {
    CHECK_EQ(consumed_, false);
    consumed_ = true;
  }

  template <class Outer>
  inline Outer* Cast() { return static_cast<Outer*>(Cast()); }

  void EmitData(ssize_t nread,
                v8::Local<v8::Object> buf,
                v8::Local<v8::Object> handle);

 protected:
  explicit StreamBase(Environment* env) : env_(env), consumed_(false) {
  }

  virtual ~StreamBase() = default;

  virtual AsyncWrap* GetAsyncWrap() = 0;

  // Libuv callbacks
  static void AfterShutdown(ShutdownWrap* req, int status);
  static void AfterWrite(WriteWrap* req, int status);

  // JS Methods
  int ReadStart(const v8::FunctionCallbackInfo<v8::Value>& args);
  int ReadStop(const v8::FunctionCallbackInfo<v8::Value>& args);
  int Shutdown(const v8::FunctionCallbackInfo<v8::Value>& args);
  int Writev(const v8::FunctionCallbackInfo<v8::Value>& args);
  int WriteBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
  template <enum encoding enc>
  int WriteString(const v8::FunctionCallbackInfo<v8::Value>& args);

  template <class Base>
  static void GetFD(v8::Local<v8::String>,
                    const v8::PropertyCallbackInfo<v8::Value>&);

  template <class Base,
            int (StreamBase::*Method)(  // NOLINT(whitespace/parens)
      const v8::FunctionCallbackInfo<v8::Value>& args)>
  static void JSMethod(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  Environment* env_;
  bool consumed_;
};

}  // namespace node

#endif  // SRC_STREAM_BASE_H_
