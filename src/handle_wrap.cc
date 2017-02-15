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
  HandleWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  if (IsAlive(wrap))
    uv_ref(wrap->GetHandle());
}


void HandleWrap::Unref(const FunctionCallbackInfo<Value>& args) {
  HandleWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  if (IsAlive(wrap))
    uv_unref(wrap->GetHandle());
}


void HandleWrap::HasRef(const FunctionCallbackInfo<Value>& args) {
  HandleWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  args.GetReturnValue().Set(HasRef(wrap));
}


void HandleWrap::Close(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  HandleWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  // Guard against uninitialized handle or double close.
  if (!IsAlive(wrap))
    return;

  if (wrap->state_ != kInitialized)
    return;

  CHECK_EQ(false, wrap->persistent().IsEmpty());
  uv_close(wrap->handle_, OnClose);
  wrap->state_ = kClosing;

  if (args[0]->IsFunction()) {
    wrap->object()->Set(env->onclose_string(), args[0]);
    wrap->state_ = kClosingWithCallback;
  }
}


HandleWrap::HandleWrap(Environment* env,
                       Local<Object> object,
                       uv_handle_t* handle,
                       AsyncWrap::ProviderType provider,
                       AsyncWrap* parent)
    : AsyncWrap(env, object, provider, parent),
      state_(kInitialized),
      handle_(handle) {
  handle_->data = this;
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
  Context::Scope context_scope(env->context());

  // The wrap object should still be there.
  CHECK_EQ(wrap->persistent().IsEmpty(), false);
  CHECK(wrap->state_ >= kClosing && wrap->state_ <= kClosingWithCallback);

  const bool have_close_callback = (wrap->state_ == kClosingWithCallback);
  wrap->state_ = kClosed;

  if (have_close_callback)
    wrap->MakeCallback(env->onclose_string(), 0, nullptr);

  ClearWrap(wrap->object());
  wrap->persistent().Reset();
  delete wrap;
}


}  // namespace node
