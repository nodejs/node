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

#include <node.h>
#include <handle_wrap.h>

namespace node {

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


#define UNWRAP \
  assert(!args.Holder().IsEmpty()); \
  assert(args.Holder()->InternalFieldCount() > 0); \
  HandleWrap* wrap =  \
      static_cast<HandleWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
  if (!wrap) { \
    uv_err_t err; \
    err.code = UV_EBADF; \
    SetErrno(err); \
    return scope.Close(Integer::New(-1)); \
  }


void HandleWrap::Initialize(Handle<Object> target) {
  /* Doesn't do anything at the moment. */
}


// This function is used only for process.stdout. It's put here instead of
// in TTYWrap because here we have access to the Close binding.
Handle<Value> HandleWrap::Unref(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  // Calling unnecessarily is a no-op
  if (wrap->unref) {
    return v8::Undefined();
  }

  wrap->unref = true;
  uv_unref(uv_default_loop());

  return v8::Undefined();
}


// Adds a reference to keep uv alive because of this thing.
Handle<Value> HandleWrap::Ref(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  // Calling multiple times is a no-op
  if (!wrap->unref) {
    return v8::Undefined();
  }

  wrap->unref = false;
  uv_ref(uv_default_loop());

  return v8::Undefined();
}


Handle<Value> HandleWrap::Close(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  // guard against uninitialized handle or double close
  if (wrap->handle__ == NULL) return v8::Null();
  assert(!wrap->object_.IsEmpty());
  uv_close(wrap->handle__, OnClose);
  wrap->handle__ = NULL;

  HandleWrap::Ref(args);

  wrap->StateChange();

  return v8::Null();
}


HandleWrap::HandleWrap(Handle<Object> object, uv_handle_t* h) {
  unref = false;
  handle__ = h;
  if (h) {
    h->data = this;
  }

  HandleScope scope;
  assert(object_.IsEmpty());
  assert(object->InternalFieldCount() > 0);
  object_ = v8::Persistent<v8::Object>::New(object);
  object_->SetPointerInInternalField(0, this);
}


void HandleWrap::SetHandle(uv_handle_t* h) {
  handle__ = h;
  h->data = this;
}


HandleWrap::~HandleWrap() {
  assert(object_.IsEmpty());
}


void HandleWrap::OnClose(uv_handle_t* handle) {
  HandleWrap* wrap = static_cast<HandleWrap*>(handle->data);

  // The wrap object should still be there.
  assert(wrap->object_.IsEmpty() == false);

  // But the handle pointer should be gone.
  assert(wrap->handle__ == NULL);

  wrap->object_->SetPointerInInternalField(0, NULL);
  wrap->object_.Dispose();
  wrap->object_.Clear();

  delete wrap;
}


}  // namespace node
