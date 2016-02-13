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

  if (wrap->flags_ & kCloseCallback)
    wrap->MakeCallback(env->onclose_string(), 0, nullptr);

  if (wrap->flags_ & kEnvironmentCleanup) {
    CHECK_EQ(wrap->hc_, nullptr);
    env->FinishHandleCleanup(handle);
  } else if (wrap->hc_ != nullptr) {
    // Prevent double clean ups.
    env->DeregisterHandleCleanup(wrap->hc_);
    wrap->hc_ = nullptr;
  }

  node::ClearWrap(object);
  wrap->persistent().Reset();
  handle->data = nullptr;
  delete wrap;
}


void HandleWrap::HandleCleanupCallback(Environment* env,
                                       uv_handle_t* handle,
                                       void* arg) {
  if (handle->data == nullptr)
    return;

  HandleWrap* wrap = static_cast<HandleWrap*>(handle->data);
  // At this point Environment has freed the HandleCleanup* and this is the
  // first opportunity to nullify it.
  // TODO(petkaantonov) it would be more symmetric with DeregisterHandleCleanup
  // if the HandleCleanup* was deleted in FinishHandleCleanup but it requires
  // changing all other users of RegisterHandleCleanup
  wrap->hc_ = nullptr;
  if (handle != nullptr && !uv_is_closing(handle)) {
    if (arg != nullptr) {
      CallbackContainer* container = static_cast<CallbackContainer*>(arg);
      container->cb_(wrap, handle);
      delete container;
    }
    // Removes CloseCallback flag as well, EnvironmentCleanup means we are
    // exiting.
    wrap->flags_ = kEnvironmentCleanup;
    uv_close(handle, OnClose);
    wrap->handle__ = nullptr;
  }
}


void HandleWrap::RegisterHandleCleanup(uv_handle_t* handle,
                                       HandleWillForceCloseCb cb) {
  CHECK_EQ(hc_, nullptr);

  CallbackContainer* container =
      cb == nullptr ? nullptr : new CallbackContainer(cb);

  hc_ = env()->RegisterHandleCleanup(handle,
                                     HandleCleanupCallback,
                                     container);
}

}  // namespace node
