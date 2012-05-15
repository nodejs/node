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

#ifndef REQ_WRAP_H_
#define REQ_WRAP_H_

#include "ngx-queue.h"

namespace node {

// defined in node.cc
extern v8::Persistent<v8::String> process_symbol;
extern v8::Persistent<v8::String> domain_symbol;
extern ngx_queue_t req_wrap_queue;

template <typename T>
class ReqWrap {
 public:
  ReqWrap() {
    v8::HandleScope scope;
    object_ = v8::Persistent<v8::Object>::New(v8::Object::New());

    v8::Local<v8::Value> domain = v8::Context::GetCurrent()
                                  ->Global()
                                  ->Get(process_symbol)
                                  ->ToObject()
                                  ->Get(domain_symbol);

    if (!domain->IsUndefined()) {
      // fprintf(stderr, "setting domain on ReqWrap\n");
      object_->Set(domain_symbol, domain);
    }

    ngx_queue_insert_tail(&req_wrap_queue, &req_wrap_queue_);
  }


  ~ReqWrap() {
    ngx_queue_remove(&req_wrap_queue_);
    // Assert that someone has called Dispatched()
    assert(req_.data == this);
    assert(!object_.IsEmpty());
    object_.Dispose();
    object_.Clear();
  }

  // Call this after the req has been dispatched.
  void Dispatched() {
    req_.data = this;
  }

  v8::Persistent<v8::Object> object_;
  ngx_queue_t req_wrap_queue_;
  void* data_;
  T req_; // *must* be last, GetActiveRequests() in node.cc depends on it
};


}  // namespace node


#endif  // REQ_WRAP_H_
