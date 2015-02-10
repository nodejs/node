#ifndef SRC_STREAM_BASE_H_
#define SRC_STREAM_BASE_H_

#include "req_wrap.h"
#include "node.h"

#include "v8.h"

namespace node {

// Forward declarations
class Environment;
class StreamBase;

class ShutdownWrap : public ReqWrap<uv_shutdown_t> {
 public:
  ShutdownWrap(Environment* env,
               v8::Local<v8::Object> req_wrap_obj,
               StreamBase* wrap)
      : ReqWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_SHUTDOWNWRAP),
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

class WriteWrap: public ReqWrap<uv_write_t> {
 public:
  // TODO(trevnorris): WrapWrap inherits from ReqWrap, which I've globbed
  // into the same provider. How should these be broken apart?
  WriteWrap(Environment* env, v8::Local<v8::Object> obj, StreamBase* wrap)
      : ReqWrap(env, obj, AsyncWrap::PROVIDER_WRITEWRAP),
        wrap_(wrap) {
    Wrap(obj, this);
  }

  void* operator new(size_t size, char* storage) { return storage; }

  // This is just to keep the compiler happy. It should never be called, since
  // we don't use exceptions in node.
  void operator delete(void* ptr, char* storage) { UNREACHABLE(); }

  inline StreamBase* wrap() const {
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

  StreamBase* const wrap_;
};

class StreamResource {
 public:
  virtual ~StreamResource() = default;

  virtual int DoShutdown(ShutdownWrap* req_wrap, uv_shutdown_cb cb) = 0;
  virtual int TryWrite(uv_buf_t** bufs, size_t* count) = 0;
  virtual int DoWrite(WriteWrap* w,
                      uv_buf_t* bufs,
                      size_t count,
                      uv_stream_t* send_handle,
                      uv_write_cb cb) = 0;
  virtual void DoAfterWrite(WriteWrap* w) = 0;
  virtual const char* Error() const = 0;
  virtual void ClearError() = 0;
};

class StreamBase : public StreamResource {
 public:
  template <class Base>
  static void AddMethods(Environment* env,
                         v8::Handle<v8::FunctionTemplate> target);

  virtual bool IsAlive() = 0;
  virtual int GetFD() = 0;

  virtual int ReadStart() = 0;
  virtual int ReadStop() = 0;
  virtual int Writev(v8::Local<v8::Object> req, v8::Local<v8::Array> bufs) = 0;
  virtual int WriteString(v8::Local<v8::Object> req,
                          v8::Local<v8::String> str,
                          enum encoding enc,
                          v8::Local<v8::Object> handle) = 0;
  virtual int SetBlocking(bool enable) = 0;

  inline bool IsConsumed() const { return consumed_; }

  // TODO(indutny): assert that stream is not yet consumed
  inline void Consume() { consumed_ = true; }

 protected:
  StreamBase(Environment* env, v8::Local<v8::Object> object);
  virtual ~StreamBase() = default;

  virtual v8::Local<v8::Object> GetObject() = 0;

  // Libuv callbacks
  static void AfterShutdown(uv_shutdown_t* req, int status);
  static void AfterWrite(uv_write_t* req, int status);

  // JS Methods
  int ReadStart(const v8::FunctionCallbackInfo<v8::Value>& args);
  int ReadStop(const v8::FunctionCallbackInfo<v8::Value>& args);
  int Shutdown(const v8::FunctionCallbackInfo<v8::Value>& args);
  int Writev(const v8::FunctionCallbackInfo<v8::Value>& args);
  int WriteBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
  template <enum encoding Encoding>
  int WriteString(const v8::FunctionCallbackInfo<v8::Value>& args);
  int SetBlocking(const v8::FunctionCallbackInfo<v8::Value>& args);

  template <class Base>
  static void GetFD(v8::Local<v8::String>,
                    const v8::PropertyCallbackInfo<v8::Value>&);

  template <class Base,
            int (StreamBase::*Method)(
      const v8::FunctionCallbackInfo<v8::Value>& args)>
  static void JSMethod(const v8::FunctionCallbackInfo<v8::Value>& args);

  bool consumed_;
};

}  // namespace node

#endif  // SRC_STREAM_BASE_H_
