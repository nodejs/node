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

class FSReqWrap: public ReqWrap<uv_fs_t> {
 public:
  enum Ownership { COPY, MOVE };

  inline static FSReqWrap* New(Environment* env,
                               Local<Object> req,
                               const char* syscall,
                               const char* data = nullptr,
                               enum encoding encoding = UTF8,
                               Ownership ownership = COPY);

  inline void Dispose();

  virtual void Reject(Local<Value> reject);
  virtual void Resolve(Local<Value> value);

  void ReleaseEarly() {
    if (data_ != inline_data()) {
      delete[] data_;
      data_ = nullptr;
    }
  }

  const char* syscall() const { return syscall_; }
  const char* data() const { return data_; }
  const enum encoding encoding_;

  size_t self_size() const override { return sizeof(*this); }

 protected:
  FSReqWrap(Environment* env,
            Local<Object> req,
            const char* syscall,
            const char* data,
            enum encoding encoding)
      : ReqWrap(env, req, AsyncWrap::PROVIDER_FSREQWRAP),
        encoding_(encoding),
        syscall_(syscall),
        data_(data) {
    Wrap(object(), this);
  }

  virtual ~FSReqWrap() {
    ReleaseEarly();
    ClearWrap(object());
  }

  void* operator new(size_t size) = delete;
  void* operator new(size_t size, char* storage) { return storage; }
  char* inline_data() { return reinterpret_cast<char*>(this + 1); }

 private:
  const char* syscall_;
  const char* data_;

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
