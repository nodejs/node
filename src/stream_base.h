#ifndef SRC_STREAM_BASE_H_
#define SRC_STREAM_BASE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

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

  inline void Done(int status, const char* error_str = nullptr) {
    Req* req = static_cast<Req*>(this);
    Environment* env = req->env();
    if (error_str != nullptr) {
      req->object()->Set(env->error_string(),
                         OneByteString(env->isolate(), error_str));
    }

    cb_(req, status);
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
  size_t self_size() const override { return sizeof(*this); }

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

  size_t self_size() const override { return storage_size_; }

  static void NewWriteWrap(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK(args.IsConstructCall());
  }

  static const size_t kAlignSize = 16;

 protected:
  WriteWrap(Environment* env,
            v8::Local<v8::Object> obj,
            StreamBase* wrap,
            DoneCb cb,
            size_t storage_size)
      : ReqWrap(env, obj, AsyncWrap::PROVIDER_WRITEWRAP),
        StreamReq<WriteWrap>(cb),
        wrap_(wrap),
        storage_size_(storage_size) {
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
  const size_t storage_size_;
};

class StreamResource {
 public:
  template <class T>
  struct Callback {
    Callback() : fn(nullptr), ctx(nullptr) {}
    Callback(T fn, void* ctx) : fn(fn), ctx(ctx) {}
    Callback(const Callback&) = default;

    inline bool is_empty() { return fn == nullptr; }
    inline void clear() {
      fn = nullptr;
      ctx = nullptr;
    }

    T fn;
    void* ctx;
  };

  typedef void (*AfterWriteCb)(WriteWrap* w, void* ctx);
  typedef void (*AllocCb)(size_t size, uv_buf_t* buf, void* ctx);
  typedef void (*ReadCb)(ssize_t nread,
                         const uv_buf_t* buf,
                         uv_handle_type pending,
                         void* ctx);

  StreamResource() : bytes_read_(0) {
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
    if (!after_write_cb_.is_empty())
      after_write_cb_.fn(w, after_write_cb_.ctx);
  }

  inline void OnAlloc(size_t size, uv_buf_t* buf) {
    if (!alloc_cb_.is_empty())
      alloc_cb_.fn(size, buf, alloc_cb_.ctx);
  }

  inline void OnRead(ssize_t nread,
                     const uv_buf_t* buf,
                     uv_handle_type pending = UV_UNKNOWN_HANDLE) {
    if (nread > 0)
      bytes_read_ += static_cast<uint64_t>(nread);
    if (!read_cb_.is_empty())
      read_cb_.fn(nread, buf, pending, read_cb_.ctx);
  }

  inline void set_after_write_cb(Callback<AfterWriteCb> c) {
    after_write_cb_ = c;
  }

  inline void set_alloc_cb(Callback<AllocCb> c) { alloc_cb_ = c; }
  inline void set_read_cb(Callback<ReadCb> c) { read_cb_ = c; }

  inline Callback<AfterWriteCb> after_write_cb() { return after_write_cb_; }
  inline Callback<AllocCb> alloc_cb() { return alloc_cb_; }
  inline Callback<ReadCb> read_cb() { return read_cb_; }

 private:
  Callback<AfterWriteCb> after_write_cb_;
  Callback<AllocCb> alloc_cb_;
  Callback<ReadCb> read_cb_;
  uint64_t bytes_read_;

  friend class StreamBase;
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
                                v8::Local<v8::FunctionTemplate> target,
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

  // One of these must be implemented
  virtual AsyncWrap* GetAsyncWrap();
  virtual v8::Local<v8::Object> GetObject();

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
  static void GetFD(v8::Local<v8::String> key,
                    const v8::PropertyCallbackInfo<v8::Value>& args);

  template <class Base>
  static void GetExternal(v8::Local<v8::String> key,
                          const v8::PropertyCallbackInfo<v8::Value>& args);

  template <class Base>
  static void GetBytesRead(v8::Local<v8::String> key,
                           const v8::PropertyCallbackInfo<v8::Value>& args);

  template <class Base,
            int (StreamBase::*Method)(
      const v8::FunctionCallbackInfo<v8::Value>& args)>
  static void JSMethod(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  Environment* env_;
  bool consumed_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_STREAM_BASE_H_
