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

}  // namespace fs

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_FILE_H_
