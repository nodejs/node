#ifndef SRC_NODE_FILE_H_
#define SRC_NODE_FILE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "req_wrap-inl.h"

namespace node {

using v8::Context;
using v8::HandleScope;
using v8::Local;
using v8::Object;
using v8::Undefined;
using v8::Value;

namespace fs {

class FSReqBase : public ReqWrap<uv_fs_t> {
 public:
  FSReqBase(Environment* env, Local<Object> req, AsyncWrap::ProviderType type)
      : ReqWrap(env, req, type) {
    Wrap(object(), this);
  }

  virtual ~FSReqBase() {
    ClearWrap(object());
  }

  void Init(const char* syscall,
            const char* data = nullptr,
            size_t len = 0,
            enum encoding encoding = UTF8) {
    syscall_ = syscall;
    encoding_ = encoding;

    if (data != nullptr) {
      CHECK_EQ(data_, nullptr);
      buffer_.AllocateSufficientStorage(len + 1);
      buffer_.SetLengthAndZeroTerminate(len);
      memcpy(*buffer_, data, len);
      data_ = *buffer_;
    }
  }

  virtual void FillStatsArray(const uv_stat_t* stat) = 0;
  virtual void Reject(Local<Value> reject) = 0;
  virtual void Resolve(Local<Value> value) = 0;
  virtual void ResolveStat() = 0;

  const char* syscall() const { return syscall_; }
  const char* data() const { return data_; }
  enum encoding encoding() const { return encoding_; }

  size_t self_size() const override { return sizeof(*this); }

 private:
  enum encoding encoding_ = UTF8;
  const char* syscall_;

  const char* data_ = nullptr;
  MaybeStackBuffer<char> buffer_;

  DISALLOW_COPY_AND_ASSIGN(FSReqBase);
};

class FSReqWrap : public FSReqBase {
 public:
  FSReqWrap(Environment* env, Local<Object> req)
      : FSReqBase(env, req, AsyncWrap::PROVIDER_FSREQWRAP) { }

  void FillStatsArray(const uv_stat_t* stat) override;
  void Reject(Local<Value> reject) override;
  void Resolve(Local<Value> value) override;
  void ResolveStat() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FSReqWrap);
};

class FSReqPromise : public FSReqBase {
 public:
  FSReqPromise(Environment* env, Local<Object> req);

  ~FSReqPromise() override;

  void FillStatsArray(const uv_stat_t* stat) override;
  void Reject(Local<Value> reject) override;
  void Resolve(Local<Value> value) override;
  void ResolveStat() override;

 private:
  bool finished_ = false;
  double statFields_[14] {};
  DISALLOW_COPY_AND_ASSIGN(FSReqPromise);
};

class FSReqAfterScope {
 public:
  FSReqAfterScope(FSReqBase* wrap, uv_fs_t* req);
  ~FSReqAfterScope();

  bool Proceed();

  void Reject(uv_fs_t* req);

 private:
  FSReqBase* wrap_ = nullptr;
  uv_fs_t* req_ = nullptr;
  HandleScope handle_scope_;
  Context::Scope context_scope_;
};

}  // namespace fs

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_FILE_H_
