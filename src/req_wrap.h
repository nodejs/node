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

#ifndef SRC_REQ_WRAP_H_
#define SRC_REQ_WRAP_H_

#include "env.h"
#include "env-inl.h"
#include "queue.h"
#include "util.h"

namespace node {

// defined in node.cc
extern QUEUE req_wrap_queue;

template <typename T>
class ReqWrap {
 public:
  ReqWrap(Environment* env,
          v8::Handle<v8::Object> object = v8::Handle<v8::Object>())
      : env_(env) {
    v8::HandleScope handle_scope(env->isolate());

    if (object.IsEmpty()) {
      object = v8::Object::New();
    }
    persistent().Reset(env->isolate(), object);

    if (env->in_domain()) {
      object->Set(env->domain_string(), env->domain_array()->Get(0));
    }

    QUEUE_INSERT_TAIL(&req_wrap_queue, &req_wrap_queue_);
  }


  ~ReqWrap() {
    QUEUE_REMOVE(&req_wrap_queue_);
    // Assert that someone has called Dispatched()
    assert(req_.data == this);
    assert(!persistent().IsEmpty());
    persistent().Dispose();
  }

  // Call this after the req has been dispatched.
  void Dispatched() {
    req_.data = this;
  }

  inline Environment* env() const {
    return env_;
  }

  inline v8::Local<v8::Object> object() {
    return PersistentToLocal(env()->isolate(), persistent());
  }

  inline v8::Persistent<v8::Object>& persistent() {
    return object_;
  }

  // TODO(bnoordhuis) Make these private.
  QUEUE req_wrap_queue_;
  T req_;  // *must* be last, GetActiveRequests() in node.cc depends on it

 private:
  v8::Persistent<v8::Object> object_;
  Environment* const env_;
};


}  // namespace node


#endif  // SRC_REQ_WRAP_H_
