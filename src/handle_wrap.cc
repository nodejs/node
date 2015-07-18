#include "handle_wrap.h"
#include "async-wrap.h"
#include "async-wrap-inl.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "node.h"

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Local;
using v8::Object;
using v8::Value;


void HandleWrap::Ref(const FunctionCallbackInfo<Value>& args) {
  HandleWrap* wrap = Unwrap<HandleWrap>(args.Holder());

  if (IsAlive(wrap)) {
    uv_ref(wrap->handle__);
    wrap->flags_ &= ~kUnref;
  }
}


void HandleWrap::Unref(const FunctionCallbackInfo<Value>& args) {
  HandleWrap* wrap = Unwrap<HandleWrap>(args.Holder());

  if (IsAlive(wrap)) {
    uv_unref(wrap->handle__);
    wrap->flags_ |= kUnref;
  }
}


void HandleWrap::Close(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  HandleWrap* wrap = Unwrap<HandleWrap>(args.Holder());

  // guard against uninitialized handle or double close
  if (!IsAlive(wrap))
    return;

  CHECK_EQ(false, wrap->persistent().IsEmpty());
  uv_close(wrap->handle__, OnClose);
  wrap->handle__ = nullptr;

  if (args[0]->IsFunction()) {
    wrap->object()->Set(env->onclose_string(), args[0]);
    wrap->flags_ |= kCloseCallback;
  }
}


HandleWrap::HandleWrap(Environment* env,
                       Local<Object> object,
                       uv_handle_t* handle,
                       AsyncWrap::ProviderType provider,
                       AsyncWrap* parent)
    : AsyncWrap(env, object, provider, parent),
      flags_(0),
      handle__(handle) {
  handle__->data = this;
  HandleScope scope(env->isolate());
  Wrap(object, this);
  env->handle_wrap_queue()->PushBack(this);
}


HandleWrap::~HandleWrap() {
  CHECK(persistent().IsEmpty());
}


void HandleWrap::OnClose(uv_handle_t* handle) {
  HandleWrap* wrap = static_cast<HandleWrap*>(handle->data);
  Environment* env = wrap->env();
  HandleScope scope(env->isolate());

  // The wrap object should still be there.
  CHECK_EQ(wrap->persistent().IsEmpty(), false);

  // But the handle pointer should be gone.
  CHECK_EQ(wrap->handle__, nullptr);

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Object> object = wrap->object();

  if (wrap->flags_ & kCloseCallback) {
    wrap->MakeCallback(env->onclose_string(), 0, nullptr);
  }

  object->SetAlignedPointerInInternalField(0, nullptr);
  wrap->persistent().Reset();
  delete wrap;
}


}  // namespace node
