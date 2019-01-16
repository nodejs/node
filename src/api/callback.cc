#include "node.h"
#include "async_wrap-inl.h"
#include "env-inl.h"
#include "v8.h"

namespace node {

using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Value;

using AsyncHooks = Environment::AsyncHooks;

CallbackScope::CallbackScope(Isolate* isolate,
                             Local<Object> object,
                             async_context asyncContext)
  : private_(new InternalCallbackScope(Environment::GetCurrent(isolate),
                                       object,
                                       asyncContext)),
    try_catch_(isolate) {
  try_catch_.SetVerbose(true);
}

CallbackScope::~CallbackScope() {
  if (try_catch_.HasCaught())
    private_->MarkAsFailed();
  delete private_;
}

InternalCallbackScope::InternalCallbackScope(AsyncWrap* async_wrap)
    : InternalCallbackScope(async_wrap->env(),
                            async_wrap->object(),
                            { async_wrap->get_async_id(),
                              async_wrap->get_trigger_async_id() }) {}

InternalCallbackScope::InternalCallbackScope(Environment* env,
                                             Local<Object> object,
                                             const async_context& asyncContext,
                                             ResourceExpectation expect)
  : env_(env),
    async_context_(asyncContext),
    object_(object),
    callback_scope_(env) {
  CHECK_IMPLIES(expect == kRequireResource, !object.IsEmpty());
  CHECK_NOT_NULL(env);

  if (!env->can_call_into_js()) {
    failed_ = true;
    return;
  }

  HandleScope handle_scope(env->isolate());
  // If you hit this assertion, you forgot to enter the v8::Context first.
  CHECK_EQ(Environment::GetCurrent(env->isolate()), env);

  if (asyncContext.async_id != 0) {
    // No need to check a return value because the application will exit if
    // an exception occurs.
    AsyncWrap::EmitBefore(env, asyncContext.async_id);
  }

  env->async_hooks()->push_async_ids(async_context_.async_id,
                               async_context_.trigger_async_id);
  pushed_ids_ = true;
}

InternalCallbackScope::~InternalCallbackScope() {
  Close();
}

void InternalCallbackScope::Close() {
  if (closed_) return;
  closed_ = true;
  HandleScope handle_scope(env_->isolate());

  if (!env_->can_call_into_js()) return;
  if (failed_ && !env_->is_main_thread() && env_->is_stopping_worker()) {
    env_->async_hooks()->clear_async_id_stack();
  }

  if (pushed_ids_)
    env_->async_hooks()->pop_async_id(async_context_.async_id);

  if (failed_) return;

  if (async_context_.async_id != 0) {
    AsyncWrap::EmitAfter(env_, async_context_.async_id);
  }

  if (env_->makecallback_depth() > 1) {
    return;
  }

  Environment::TickInfo* tick_info = env_->tick_info();

  if (!env_->can_call_into_js()) return;
  if (!tick_info->has_tick_scheduled()) {
    env_->isolate()->RunMicrotasks();
  }

  // Make sure the stack unwound properly. If there are nested MakeCallback's
  // then it should return early and not reach this code.
  if (env_->async_hooks()->fields()[AsyncHooks::kTotals]) {
    CHECK_EQ(env_->execution_async_id(), 0);
    CHECK_EQ(env_->trigger_async_id(), 0);
  }

  if (!tick_info->has_tick_scheduled() && !tick_info->has_rejection_to_warn()) {
    return;
  }

  Local<Object> process = env_->process_object();

  if (!env_->can_call_into_js()) return;

  Local<Function> tick_callback = env_->tick_callback_function();

  // The tick is triggered before JS land calls SetTickCallback
  // to initializes the tick callback during bootstrap.
  CHECK(!tick_callback.IsEmpty());

  if (tick_callback->Call(env_->context(), process, 0, nullptr).IsEmpty()) {
    failed_ = true;
  }
}

MaybeLocal<Value> InternalMakeCallback(Environment* env,
                                       Local<Object> recv,
                                       const Local<Function> callback,
                                       int argc,
                                       Local<Value> argv[],
                                       async_context asyncContext) {
  CHECK(!recv.IsEmpty());
  InternalCallbackScope scope(env, recv, asyncContext);
  if (scope.Failed()) {
    return MaybeLocal<Value>();
  }

  Local<Function> domain_cb = env->domain_callback();
  MaybeLocal<Value> ret;
  if (asyncContext.async_id != 0 || domain_cb.IsEmpty() || recv.IsEmpty()) {
    ret = callback->Call(env->context(), recv, argc, argv);
  } else {
    std::vector<Local<Value>> args(1 + argc);
    args[0] = callback;
    std::copy(&argv[0], &argv[argc], args.begin() + 1);
    ret = domain_cb->Call(env->context(), recv, args.size(), &args[0]);
  }

  if (ret.IsEmpty()) {
    scope.MarkAsFailed();
    return MaybeLocal<Value>();
  }

  scope.Close();
  if (scope.Failed()) {
    return MaybeLocal<Value>();
  }

  return ret;
}

// Public MakeCallback()s

MaybeLocal<Value> MakeCallback(Isolate* isolate,
                               Local<Object> recv,
                               const char* method,
                               int argc,
                               Local<Value> argv[],
                               async_context asyncContext) {
  Local<String> method_string =
      String::NewFromUtf8(isolate, method, NewStringType::kNormal)
          .ToLocalChecked();
  return MakeCallback(isolate, recv, method_string, argc, argv, asyncContext);
}

MaybeLocal<Value> MakeCallback(Isolate* isolate,
                               Local<Object> recv,
                               Local<String> symbol,
                               int argc,
                               Local<Value> argv[],
                               async_context asyncContext) {
  Local<Value> callback_v =
      recv->Get(isolate->GetCurrentContext(), symbol).ToLocalChecked();
  if (callback_v.IsEmpty()) return Local<Value>();
  if (!callback_v->IsFunction()) return Local<Value>();
  Local<Function> callback = callback_v.As<Function>();
  return MakeCallback(isolate, recv, callback, argc, argv, asyncContext);
}

MaybeLocal<Value> MakeCallback(Isolate* isolate,
                               Local<Object> recv,
                               Local<Function> callback,
                               int argc,
                               Local<Value> argv[],
                               async_context asyncContext) {
  // Observe the following two subtleties:
  //
  // 1. The environment is retrieved from the callback function's context.
  // 2. The context to enter is retrieved from the environment.
  //
  // Because of the AssignToContext() call in src/node_contextify.cc,
  // the two contexts need not be the same.
  Environment* env = Environment::GetCurrent(callback->CreationContext());
  CHECK_NOT_NULL(env);
  Context::Scope context_scope(env->context());
  MaybeLocal<Value> ret =
      InternalMakeCallback(env, recv, callback, argc, argv, asyncContext);
  if (ret.IsEmpty() && env->makecallback_depth() == 0) {
    // This is only for legacy compatiblity and we may want to look into
    // removing/adjusting it.
    return Undefined(env->isolate());
  }
  return ret;
}

// Legacy MakeCallback()s

Local<Value> MakeCallback(Isolate* isolate,
                          Local<Object> recv,
                          const char* method,
                          int argc,
                          Local<Value>* argv) {
  EscapableHandleScope handle_scope(isolate);
  return handle_scope.Escape(
      MakeCallback(isolate, recv, method, argc, argv, {0, 0})
          .FromMaybe(Local<Value>()));
}

Local<Value> MakeCallback(Isolate* isolate,
                          Local<Object> recv,
                          Local<String> symbol,
                          int argc,
                          Local<Value>* argv) {
  EscapableHandleScope handle_scope(isolate);
  return handle_scope.Escape(
      MakeCallback(isolate, recv, symbol, argc, argv, {0, 0})
          .FromMaybe(Local<Value>()));
}

Local<Value> MakeCallback(Isolate* isolate,
                          Local<Object> recv,
                          Local<Function> callback,
                          int argc,
                          Local<Value>* argv) {
  EscapableHandleScope handle_scope(isolate);
  return handle_scope.Escape(
      MakeCallback(isolate, recv, callback, argc, argv, {0, 0})
          .FromMaybe(Local<Value>()));
}

}  // namespace node
