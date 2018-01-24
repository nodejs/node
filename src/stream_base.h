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
class StreamResource;

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


// This is the generic interface for objects that control Node.js' C++ streams.
// For example, the default `EmitToJSStreamListener` emits a stream's data
// as Buffers in JS, or `TLSWrap` reads and decrypts data from a stream.
class StreamListener {
 public:
  virtual ~StreamListener();

  // This is called when a stream wants to allocate memory immediately before
  // reading data into the freshly allocated buffer (i.e. it is always followed
  // by a `OnStreamRead()` call).
  // This memory may be statically or dynamically allocated; for example,
  // a protocol parser may want to read data into a static buffer if it knows
  // that all data is going to be fully handled during the next
  // `OnStreamRead()` call.
  // The returned buffer does not need to contain `suggested_size` bytes.
  // The default implementation of this method returns a buffer that has exactly
  // the suggested size and is allocated using malloc().
  virtual uv_buf_t OnStreamAlloc(size_t suggested_size);

  // `OnStreamRead()` is called when data is available on the socket and has
  // been read into the buffer provided by `OnStreamAlloc()`.
  // The `buf` argument is the return value of `uv_buf_t`, or may be a buffer
  // with base nullpptr in case of an error.
  // `nread` is the number of read bytes (which is at most the buffer length),
  // or, if negative, a libuv error code.
  virtual void OnStreamRead(ssize_t nread,
                            const uv_buf_t& buf) = 0;

  // This is called once a Write has finished. `status` may be 0 or,
  // if negative, a libuv error code.
  virtual void OnStreamAfterWrite(WriteWrap* w, int status) {}

  // This is called immediately before the stream is destroyed.
  virtual void OnStreamDestroy() {}

 protected:
  // Pass along a read error to the `StreamListener` instance that was active
  // before this one. For example, a protocol parser does not care about read
  // errors and may instead want to let the original handler
  // (e.g. the JS handler) take care of the situation.
  void PassReadErrorToPreviousListener(ssize_t nread);

  StreamResource* stream_ = nullptr;
  StreamListener* previous_listener_ = nullptr;

  friend class StreamResource;
};


// A default emitter that just pushes data chunks as Buffer instances to
// JS land via the handle’s .ondata method.
class EmitToJSStreamListener : public StreamListener {
 public:
  void OnStreamRead(ssize_t nread, const uv_buf_t& buf) override;
};


// A generic stream, comparable to JS land’s `Duplex` streams.
// A stream is always controlled through one `StreamListener` instance.
class StreamResource {
 public:
  virtual ~StreamResource();

  virtual int DoShutdown(ShutdownWrap* req_wrap) = 0;
  virtual int DoTryWrite(uv_buf_t** bufs, size_t* count);
  virtual int DoWrite(WriteWrap* w,
                      uv_buf_t* bufs,
                      size_t count,
                      uv_stream_t* send_handle) = 0;
  virtual bool HasWriteQueue();

  // Start reading from the underlying resource. This is called by the consumer
  // when more data is desired.
  virtual int ReadStart() = 0;
  // Stop reading from the underlying resource. This is called by the
  // consumer when its buffers are full and no more data can be handled.
  virtual int ReadStop() = 0;

  // Optionally, this may provide an error message to be used for
  // failing writes.
  virtual const char* Error() const;
  // Clear the current error (i.e. that would be returned by Error()).
  virtual void ClearError();

  // Transfer ownership of this tream to `listener`. The previous listener
  // will not receive any more callbacks while the new listener was active.
  void PushStreamListener(StreamListener* listener);
  // Remove a listener, and, if this was the currently active one,
  // transfer ownership back to the previous listener.
  void RemoveStreamListener(StreamListener* listener);

 protected:
  // Call the current listener's OnStreamAlloc() method.
  uv_buf_t EmitAlloc(size_t suggested_size);
  // Call the current listener's OnStreamRead() method and update the
  // stream's read byte counter.
  void EmitRead(ssize_t nread, const uv_buf_t& buf = uv_buf_init(nullptr, 0));
  // Call the current listener's OnStreamAfterWrite() method.
  void EmitAfterWrite(WriteWrap* w, int status);

  StreamListener* listener_ = nullptr;
  uint64_t bytes_read_ = 0;

  friend class StreamListener;
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

  void CallJSOnreadMethod(ssize_t nread, v8::Local<v8::Object> buf);

  // These are called by the respective {Write,Shutdown}Wrap class.
  virtual void AfterShutdown(ShutdownWrap* req, int status);
  virtual void AfterWrite(WriteWrap* req, int status);

  // This is named `stream_env` to avoid name clashes, because a lot of
  // subclasses are also `BaseObject`s.
  Environment* stream_env() const;

 protected:
  explicit StreamBase(Environment* env);

  // One of these must be implemented
  virtual AsyncWrap* GetAsyncWrap() = 0;
  virtual v8::Local<v8::Object> GetObject();

  // JS Methods
  int ReadStartJS(const v8::FunctionCallbackInfo<v8::Value>& args);
  int ReadStopJS(const v8::FunctionCallbackInfo<v8::Value>& args);
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
  EmitToJSStreamListener default_listener_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_STREAM_BASE_H_
