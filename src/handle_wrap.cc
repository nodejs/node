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

#include "node.h"
#include "ngx-queue.h"
#include "handle_wrap.h"

#define UNWRAP_NO_ABORT(type)                                               \
  assert(!args.Holder().IsEmpty());                                         \
  assert(args.Holder()->InternalFieldCount() > 0);                          \
  type* wrap = static_cast<type*>(                                          \
      args.Holder()->GetAlignedPointerFromInternalField(0));

namespace node {

using v8::Array;
using v8::Object;
using v8::Handle;
using v8::Local;
using v8::Persistent;
using v8::Value;
using v8::HandleScope;
using v8::FunctionTemplate;
using v8::String;
using v8::Function;
using v8::TryCatch;
using v8::Context;
using v8::Arguments;
using v8::Integer;


// defined in node.cc
extern ngx_queue_t handle_wrap_queue;


void HandleWrap::Initialize(Handle<Object> target) {
  /* Doesn't do anything at the moment. */
}


Handle<Value> HandleWrap::Ref(const Arguments& args) {
  HandleScope scope;

  UNWRAP_NO_ABORT(HandleWrap)

  if (wrap) {
    uv_ref(wrap->handle__);
    wrap->unref_ = false;
  }

  return v8::Undefined();
}


Handle<Value> HandleWrap::Unref(const Arguments& args) {
  HandleScope scope;

  UNWRAP_NO_ABORT(HandleWrap)

  if (wrap) {
    uv_unref(wrap->handle__);
    wrap->unref_ = true;
  }

  return v8::Undefined();
}


Handle<Value> HandleWrap::Close(const Arguments& args) {
  HandleScope scope;

  HandleWrap *wrap = static_cast<HandleWrap*>(
      args.Holder()->GetAlignedPointerFromInternalField(0));

  // guard against uninitialized handle or double close
  if (wrap && wrap->handle__) {
    assert(!wrap->object_.IsEmpty());
    uv_close(wrap->handle__, OnClose);
    wrap->handle__ = NULL;
  }

  return v8::Null();
}


HandleWrap::HandleWrap(Handle<Object> object, uv_handle_t* h) {
  unref_ = false;
  handle__ = h;
  if (h) {
    h->data = this;
  }

  HandleScope scope;
  assert(object_.IsEmpty());
  assert(object->InternalFieldCount() > 0);
  object_ = v8::Persistent<v8::Object>::New(object);
  object_->SetAlignedPointerInInternalField(0, this);
  ngx_queue_insert_tail(&handle_wrap_queue, &handle_wrap_queue_);
}


void HandleWrap::SetHandle(uv_handle_t* h) {
  handle__ = h;
  h->data = this;
}


HandleWrap::~HandleWrap() {
  assert(object_.IsEmpty());
  ngx_queue_remove(&handle_wrap_queue_);
}


void HandleWrap::OnClose(uv_handle_t* handle) {
  HandleWrap* wrap = static_cast<HandleWrap*>(handle->data);

  // The wrap object should still be there.
  assert(wrap->object_.IsEmpty() == false);

  // But the handle pointer should be gone.
  assert(wrap->handle__ == NULL);

  wrap->object_->SetAlignedPointerInInternalField(0, NULL);
  wrap->object_.Dispose();
  wrap->object_.Clear();

  delete wrap;
}


}  // namespace node
