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
    SetErrno(UV_EBADF); \
    return scope.Close(Integer::New(-1)); \
  }


void HandleWrap::Initialize(Handle<Object> target) {
  /* Doesn't do anything at the moment. */
}


Handle<Value> HandleWrap::Close(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  assert(!wrap->object_.IsEmpty());
  uv_close(wrap->handle__, OnClose);

  wrap->StateChange();

  return v8::Null();
}


HandleWrap::HandleWrap(Handle<Object> object, uv_handle_t* h) {
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

  wrap->object_->SetPointerInInternalField(0, NULL);
  wrap->object_.Dispose();
  wrap->object_.Clear();

  delete wrap;
}


}  // namespace node
