// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_NODE_FILE_H_
#define SRC_NODE_FILE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "req_wrap-inl.h"

namespace node {

using v8::Local;
using v8::Object;
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

}  // namespace fs

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_FILE_H_
