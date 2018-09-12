#include "node.h"
#include "async_wrap-inl.h"
#include "env-inl.h"
#include "v8.h"

namespace node {

using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;

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

  CHECK_GE(env->makecallback_depth(), 1);
  if (env->makecallback_depth() == 1) {
    env->tick_info()->set_has_thrown(false);
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
  if (!tick_info->has_scheduled()) {
    env_->isolate()->RunMicrotasks();
  }

  // Make sure the stack unwound properly. If there are nested MakeCallback's
  // then it should return early and not reach this code.
  if (env_->async_hooks()->fields()[AsyncHooks::kTotals]) {
    CHECK_EQ(env_->execution_async_id(), 0);
    CHECK_EQ(env_->trigger_async_id(), 0);
  }

  if (!tick_info->has_scheduled() && !tick_info->has_promise_rejections()) {
    return;
  }

  Local<Object> process = env_->process_object();

  if (!env_->can_call_into_js()) return;

  if (env_->tick_callback_function()->Call(process, 0, nullptr).IsEmpty()) {
    env_->tick_info()->set_has_thrown(true);
    failed_ = true;
  }
}

}  // namespace node
