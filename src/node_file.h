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
using v8::MaybeLocal;
using v8::Object;
using v8::Persistent;
using v8::Promise;
using v8::PropertyCallbackInfo;
using v8::String;
using v8::Undefined;
using v8::Value;

namespace fs {

enum class Ownership { COPY, MOVE };

class FSReqInfo {
 public:
  FSReqInfo(const char* syscall,
            const char* data,
            enum encoding encoding)
      : encoding_(encoding),
        syscall_(syscall),
        data_(data) { }

  virtual ~FSReqInfo() {
    Release();
  }

  inline void Dispose();

  void Release() {
    if (data_ != inline_data()) {
      delete[] data_;
      data_ = nullptr;
    }
  }

  const char* syscall() const { return syscall_; }
  const char* data() const { return data_; }
  void SetData(const char* data) { data_ = data; }
  const enum encoding encoding_;

  void* operator new(size_t size) = delete;
  void* operator new(size_t size, char* storage) { return storage; }
  char* inline_data() { return reinterpret_cast<char*>(this + 1); }

 private:
  const char* syscall_;
  const char* data_;

  DISALLOW_COPY_AND_ASSIGN(FSReqInfo);
};

class FSReqWrap : public ReqWrap<uv_fs_t> {
 public:
  FSReqWrap(Environment* env, Local<Object> req)
      : ReqWrap(env, req, AsyncWrap::PROVIDER_FSREQWRAP) {
    Wrap(object(), this);
  }

  virtual ~FSReqWrap() {
    ReleaseEarly();
    ClearWrap(object());
  }

  inline void Init(const char* syscall,
                   const char* data = nullptr,
                   enum encoding encoding = UTF8,
                   Ownership ownership = Ownership::COPY);

  inline void Dispose();

  virtual void FillStatsArray(const uv_stat_t* stat);
  virtual void Reject(Local<Value> reject);
  virtual void Resolve(Local<Value> value);
  virtual void ResolveStat();

  void ReleaseEarly() {
    if (info_ != nullptr)
      info_->Release();
  }

  const char* syscall() const {
    return info_ != nullptr ? info_->syscall() : nullptr;
  }

  const char* data() const {
    return info_ != nullptr ? info_->data() : nullptr;
  }

  enum encoding encoding() const {
    return info_ != nullptr ? info_->encoding_ : UTF8;
  }

  size_t self_size() const override { return sizeof(*this); }

 private:
  FSReqInfo* info_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FSReqWrap);
};

class FSReqPromise : public FSReqWrap {
 public:
  FSReqPromise(Environment* env, Local<Object> req)
    : FSReqWrap(env, req) {
    MaybeLocal<Promise::Resolver> promise =
        Promise::Resolver::New(env->context());
    object()->Set(env->context(),
                  env->promise_string(),
                  promise.ToLocalChecked());
  }

  ~FSReqPromise() override {}

  void FillStatsArray(const uv_stat_t* stat) override;
  void Reject(Local<Value> reject) override;
  void Resolve(Local<Value> value) override;
  void ResolveStat() override;
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
// the object is garbage collected.
class FD : public AsyncWrap {
 public:
  FD(Environment* env, int fd);
  virtual ~FD();

  int fd() const { return fd_; }
  size_t self_size() const override { return sizeof(*this); }

  static void GetFD(Local<String> property,
                     const PropertyCallbackInfo<Value>& info);
 private:
  void Close();

  int fd_;
  v8::Persistent<v8::Promise> promise_;
};

}  // namespace fs

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_FILE_H_
