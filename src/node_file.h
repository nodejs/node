#ifndef SRC_NODE_FILE_H_
#define SRC_NODE_FILE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "req_wrap-inl.h"

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Local;
using v8::Object;
using v8::Persistent;
using v8::Promise;
using v8::Undefined;
using v8::Value;

namespace fs {

class FSReqWrap : public ReqWrap<uv_fs_t> {
 public:
  FSReqWrap(Environment* env, Local<Object> req)
      : ReqWrap(env, req, AsyncWrap::PROVIDER_FSREQWRAP) {
    Wrap(object(), this);
  }

  virtual ~FSReqWrap() {
    ClearWrap(object());
  }

  void Init(const char* syscall,
            const char* data = nullptr,
            size_t len = 0,
            enum encoding encoding = UTF8);

  virtual void FillStatsArray(const uv_stat_t* stat);
  virtual void Reject(Local<Value> reject);
  virtual void Resolve(Local<Value> value);
  virtual void ResolveStat();

  const char* syscall() const { return syscall_; }
  const char* data() const { return data_; }
  enum encoding encoding() const { return encoding_; }

  size_t self_size() const override { return sizeof(*this); }

 private:
  enum encoding encoding_ = UTF8;
  const char* syscall_;

  const char* data_ = nullptr;
  MaybeStackBuffer<char> buffer_;

  DISALLOW_COPY_AND_ASSIGN(FSReqWrap);
};

class FSReqAfterScope {
 public:
  FSReqAfterScope(FSReqWrap* wrap, uv_fs_t* req);
  ~FSReqAfterScope();

  bool Proceed();

  void Reject(uv_fs_t* req);

 private:
  FSReqWrap* wrap_ = nullptr;
  uv_fs_t* req_ = nullptr;
  HandleScope handle_scope_;
  Context::Scope context_scope_;
};

// A wrapper for a file descriptor that will automatically close the fd when
// the object is garbage collected
class FD : public AsyncWrap {
 public:
  FD(Environment* env, int fd);
  virtual ~FD();

  int fd() const { return fd_; }
  size_t self_size() const override { return sizeof(*this); }

  // Will asynchronously close the FD and return a Promise that will
  // be resolved once closing is complete.
  static void Close(const FunctionCallbackInfo<Value>& args);

 private:
  // Synchronous close that emits a warning
  inline void Close();

  class CloseReq {
   public:
    CloseReq(Environment* env,
             Local<Promise> promise,
             Local<Value> ref) : env_(env) {
      promise_.Reset(env->isolate(), promise);
      ref_.Reset(env->isolate(), ref);
    }
    ~CloseReq() {
      uv_fs_req_cleanup(&req);
      promise_.Empty();
      ref_.Empty();
    }

    Environment* env() const { return env_; }

    FD* fd();

    void Resolve();

    void Reject(Local<Value> reason);

    uv_fs_t req;

   private:
    Environment* env_;
    Persistent<Promise> promise_;
    Persistent<Value> ref_;
  };

  // Asynchronous close
  inline Local<Promise> ClosePromise();

  int fd_;
  bool closing_ = false;
  bool closed_ = false;

  DISALLOW_COPY_AND_ASSIGN(FD);
};

}  // namespace fs

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_FILE_H_
