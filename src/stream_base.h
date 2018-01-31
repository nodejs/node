#ifndef SRC_STREAM_BASE_H_
#define SRC_STREAM_BASE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "async_wrap.h"
#include "req_wrap-inl.h"
#include "node.h"
#include "util.h"

#include "v8.h"

namespace node {

// Forward declarations
class StreamBase;

template<typename Base>
class StreamReq {
 public:
  explicit StreamReq(StreamBase* stream) : stream_(stream) {
  }

  inline void Done(int status, const char* error_str = nullptr) {
    Base* req = static_cast<Base*>(this);
    Environment* env = req->env();
    if (error_str != nullptr) {
      req->object()->Set(env->error_string(),
                         OneByteString(env->isolate(), error_str));
    }

    req->OnDone(status);
  }

  inline StreamBase* stream() const { return stream_; }

 private:
  StreamBase* const stream_;
};

class ShutdownWrap : public ReqWrap<uv_shutdown_t>,
                     public StreamReq<ShutdownWrap> {
 public:
  ShutdownWrap(Environment* env,
               v8::Local<v8::Object> req_wrap_obj,
               StreamBase* stream)
      : ReqWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_SHUTDOWNWRAP),
        StreamReq<ShutdownWrap>(stream) {
    Wrap(req_wrap_obj, this);
  }

  ~ShutdownWrap() {
    ClearWrap(object());
  }

  static ShutdownWrap* from_req(uv_shutdown_t* req) {
    return ContainerOf(&ShutdownWrap::req_, req);
  }

  size_t self_size() const override { return sizeof(*this); }

  inline void OnDone(int status);  // Just calls stream()->AfterShutdown()
};

class WriteWrap : public ReqWrap<uv_write_t>,
                  public StreamReq<WriteWrap> {
 public:
  static inline WriteWrap* New(Environment* env,
                               v8::Local<v8::Object> obj,
                               StreamBase* stream,
                               size_t extra = 0);
  inline void Dispose();
  inline char* Extra(size_t offset = 0);
  inline size_t ExtraSize() const;

  size_t self_size() const override { return storage_size_; }

  static WriteWrap* from_req(uv_write_t* req) {
    return ContainerOf(&WriteWrap::req_, req);
  }

  static const size_t kAlignSize = 16;

  WriteWrap(Environment* env,
            v8::Local<v8::Object> obj,
            StreamBase* stream)
      : ReqWrap(env, obj, AsyncWrap::PROVIDER_WRITEWRAP),
        StreamReq<WriteWrap>(stream),
        storage_size_(0) {
    Wrap(obj, this);
  }

  inline void OnDone(int status);  // Just calls stream()->AfterWrite()

 protected:
  WriteWrap(Environment* env,
            v8::Local<v8::Object> obj,
            StreamBase* stream,
            size_t storage_size)
      : ReqWrap(env, obj, AsyncWrap::PROVIDER_WRITEWRAP),
        StreamReq<WriteWrap>(stream),
        storage_size_(storage_size) {
    Wrap(obj, this);
  }

  ~WriteWrap() {
    ClearWrap(object());
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

  typedef void (*AfterWriteCb)(WriteWrap* w, int status, void* ctx);
  typedef void (*AllocCb)(size_t size, uv_buf_t* buf, void* ctx);
  typedef void (*ReadCb)(ssize_t nread,
                         const uv_buf_t* buf,
                         uv_handle_type pending,
                         void* ctx);
  typedef void (*DestructCb)(void* ctx);

  StreamResource() : bytes_read_(0) {
  }
  virtual ~StreamResource() {
    if (!destruct_cb_.is_empty())
      destruct_cb_.fn(destruct_cb_.ctx);
  }

  virtual int DoShutdown(ShutdownWrap* req_wrap) = 0;
  virtual int DoTryWrite(uv_buf_t** bufs, size_t* count);
  virtual int DoWrite(WriteWrap* w,
                      uv_buf_t* bufs,
                      size_t count,
                      uv_stream_t* send_handle) = 0;
  virtual const char* Error() const;
  virtual void ClearError();

  // Events
  inline void EmitAfterWrite(WriteWrap* w, int status) {
    if (!after_write_cb_.is_empty())
      after_write_cb_.fn(w, status, after_write_cb_.ctx);
  }

  inline void EmitAlloc(size_t size, uv_buf_t* buf) {
    if (!alloc_cb_.is_empty())
      alloc_cb_.fn(size, buf, alloc_cb_.ctx);
  }

  inline void EmitRead(ssize_t nread,
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
  inline void set_destruct_cb(Callback<DestructCb> c) { destruct_cb_ = c; }

  inline Callback<AfterWriteCb> after_write_cb() { return after_write_cb_; }
  inline Callback<AllocCb> alloc_cb() { return alloc_cb_; }
  inline Callback<ReadCb> read_cb() { return read_cb_; }
  inline Callback<DestructCb> destruct_cb() { return destruct_cb_; }

 protected:
  Callback<AfterWriteCb> after_write_cb_;
  Callback<AllocCb> alloc_cb_;
  Callback<ReadCb> read_cb_;
  Callback<DestructCb> destruct_cb_;
  uint64_t bytes_read_;
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

  inline void Unconsume() {
    CHECK_EQ(consumed_, true);
    consumed_ = false;
  }

  void EmitData(ssize_t nread,
                v8::Local<v8::Object> buf,
                v8::Local<v8::Object> handle);

  // These are called by the respective {Write,Shutdown}Wrap class.
  virtual void AfterShutdown(ShutdownWrap* req, int status);
  virtual void AfterWrite(WriteWrap* req, int status);

 protected:
  explicit StreamBase(Environment* env) : env_(env), consumed_(false) {
  }

  virtual ~StreamBase() = default;

  // One of these must be implemented
  virtual AsyncWrap* GetAsyncWrap() = 0;
  virtual v8::Local<v8::Object> GetObject();

  // JS Methods
  int ReadStart(const v8::FunctionCallbackInfo<v8::Value>& args);
  int ReadStop(const v8::FunctionCallbackInfo<v8::Value>& args);
  int Shutdown(const v8::FunctionCallbackInfo<v8::Value>& args);
  int Writev(const v8::FunctionCallbackInfo<v8::Value>& args);
  int WriteBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
  template <enum encoding enc>
  int WriteString(const v8::FunctionCallbackInfo<v8::Value>& args);

  template <class Base>
  static void GetFD(const v8::FunctionCallbackInfo<v8::Value>& args);

  template <class Base>
  static void GetExternal(const v8::FunctionCallbackInfo<v8::Value>& args);

  template <class Base>
  static void GetBytesRead(const v8::FunctionCallbackInfo<v8::Value>& args);

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
