// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/d8/async-hooks-wrapper.h"

#include "include/v8-function.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-template.h"
#include "src/api/api-inl.h"
#include "src/api/api.h"
#include "src/d8/d8.h"
#include "src/execution/isolate-inl.h"
#include "src/objects/managed-inl.h"

namespace v8 {

namespace {
std::shared_ptr<AsyncHooksWrap> UnwrapHook(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  Local<Object> hook = args.This();

  AsyncHooks* hooks = PerIsolateData::Get(isolate)->GetAsyncHooks();

  if (!hooks->async_hook_ctor.Get(isolate)->HasInstance(hook)) {
    isolate->ThrowError("Invalid 'this' passed instead of AsyncHooks instance");
    return nullptr;
  }

  i::Handle<i::Object> handle = Utils::OpenHandle(*hook->GetInternalField(0));
  return i::Handle<i::Managed<AsyncHooksWrap>>::cast(handle)->get();
}

void EnableHook(const v8::FunctionCallbackInfo<v8::Value>& args) {
  auto wrap = UnwrapHook(args);
  if (wrap) wrap->Enable();
}

void DisableHook(const v8::FunctionCallbackInfo<v8::Value>& args) {
  auto wrap = UnwrapHook(args);
  if (wrap) wrap->Disable();
}

}  // namespace

AsyncHooks::AsyncHooks(Isolate* isolate) : isolate_(isolate) {
  AsyncContext ctx;
  ctx.execution_async_id = 1;
  ctx.trigger_async_id = 0;
  asyncContexts.push(ctx);
  current_async_id = 1;

  HandleScope handle_scope(isolate_);

  async_hook_ctor.Reset(isolate_, FunctionTemplate::New(isolate_));
  async_hook_ctor.Get(isolate_)->SetClassName(
      String::NewFromUtf8Literal(isolate_, "AsyncHook"));

  async_hooks_templ.Reset(isolate_,
                          async_hook_ctor.Get(isolate_)->InstanceTemplate());
  async_hooks_templ.Get(isolate_)->SetInternalFieldCount(1);
  async_hooks_templ.Get(isolate_)->Set(
      isolate_, "enable", FunctionTemplate::New(isolate_, EnableHook));
  async_hooks_templ.Get(isolate_)->Set(
      isolate_, "disable", FunctionTemplate::New(isolate_, DisableHook));

  async_id_smb.Reset(isolate_, Private::New(isolate_));
  trigger_id_smb.Reset(isolate_, Private::New(isolate_));

  isolate_->SetPromiseHook(ShellPromiseHook);
}

AsyncHooks::~AsyncHooks() {
  isolate_->SetPromiseHook(nullptr);
  base::RecursiveMutexGuard lock_guard(&async_wraps_mutex_);
  async_wraps_.clear();
}

void AsyncHooksWrap::Enable() { enabled_ = true; }

void AsyncHooksWrap::Disable() { enabled_ = false; }

v8::Local<v8::Function> AsyncHooksWrap::init_function() const {
  return init_function_.Get(isolate_);
}
void AsyncHooksWrap::set_init_function(v8::Local<v8::Function> value) {
  init_function_.Reset(isolate_, value);
}
v8::Local<v8::Function> AsyncHooksWrap::before_function() const {
  return before_function_.Get(isolate_);
}
void AsyncHooksWrap::set_before_function(v8::Local<v8::Function> value) {
  before_function_.Reset(isolate_, value);
}
v8::Local<v8::Function> AsyncHooksWrap::after_function() const {
  return after_function_.Get(isolate_);
}
void AsyncHooksWrap::set_after_function(v8::Local<v8::Function> value) {
  after_function_.Reset(isolate_, value);
}
v8::Local<v8::Function> AsyncHooksWrap::promiseResolve_function() const {
  return promiseResolve_function_.Get(isolate_);
}
void AsyncHooksWrap::set_promiseResolve_function(
    v8::Local<v8::Function> value) {
  promiseResolve_function_.Reset(isolate_, value);
}

async_id_t AsyncHooks::GetExecutionAsyncId() const {
  return asyncContexts.top().execution_async_id;
}

async_id_t AsyncHooks::GetTriggerAsyncId() const {
  return asyncContexts.top().trigger_async_id;
}

Local<Object> AsyncHooks::CreateHook(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  EscapableHandleScope handle_scope(isolate);

  Local<Context> currentContext = isolate->GetCurrentContext();

  if (args.Length() != 1 || !args[0]->IsObject()) {
    isolate->ThrowError("Invalid arguments passed to createHook");
    return Local<Object>();
  }

  std::shared_ptr<AsyncHooksWrap> wrap =
      std::make_shared<AsyncHooksWrap>(isolate);

  Local<Object> fn_obj = args[0].As<Object>();

#define SET_HOOK_FN(name)                                                      \
  MaybeLocal<Value> name##_maybe_func =                                        \
      fn_obj->Get(currentContext, String::NewFromUtf8Literal(isolate, #name)); \
  Local<Value> name##_func;                                                    \
  if (name##_maybe_func.ToLocal(&name##_func) && name##_func->IsFunction()) {  \
    wrap->set_##name##_function(name##_func.As<Function>());                   \
  }

  SET_HOOK_FN(init);
  SET_HOOK_FN(before);
  SET_HOOK_FN(after);
  SET_HOOK_FN(promiseResolve);
#undef SET_HOOK_FN

  Local<Object> obj = async_hooks_templ.Get(isolate)
                          ->NewInstance(currentContext)
                          .ToLocalChecked();
  i::Handle<i::Object> managed = i::Managed<AsyncHooksWrap>::FromSharedPtr(
      reinterpret_cast<i::Isolate*>(isolate), sizeof(AsyncHooksWrap), wrap);
  obj->SetInternalField(0, Utils::ToLocal(managed));

  {
    base::RecursiveMutexGuard lock_guard(&async_wraps_mutex_);
    async_wraps_.push_back(std::move(wrap));
  }

  return handle_scope.Escape(obj);
}

void AsyncHooks::ShellPromiseHook(PromiseHookType type, Local<Promise> promise,
                                  Local<Value> parent) {
  v8::Isolate* isolate = promise->GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  AsyncHooks* hooks = PerIsolateData::Get(isolate)->GetAsyncHooks();
  HandleScope handle_scope(isolate);
  // Temporarily clear any scheduled_exception to allow evaluating JS that can
  // throw.
  i::Handle<i::Object> scheduled_exception;
  if (i_isolate->has_scheduled_exception()) {
    scheduled_exception = handle(i_isolate->scheduled_exception(), i_isolate);
    i_isolate->clear_scheduled_exception();
  }
  {
    TryCatch try_catch(isolate);
    try_catch.SetVerbose(true);

    Local<Context> currentContext = isolate->GetCurrentContext();
    DCHECK(!currentContext.IsEmpty());

    if (type == PromiseHookType::kInit) {
      ++hooks->current_async_id;
      Local<Integer> async_id = Integer::New(isolate, hooks->current_async_id);
      CHECK(
          !promise->HasPrivate(currentContext, hooks->async_id_smb.Get(isolate))
               .ToChecked());
      promise->SetPrivate(currentContext, hooks->async_id_smb.Get(isolate),
                          async_id);

      if (parent->IsPromise()) {
        Local<Promise> parent_promise = parent.As<Promise>();
        Local<Value> parent_async_id =
            parent_promise
                ->GetPrivate(currentContext, hooks->async_id_smb.Get(isolate))
                .ToLocalChecked();
        promise->SetPrivate(currentContext, hooks->trigger_id_smb.Get(isolate),
                            parent_async_id);
      } else {
        CHECK(parent->IsUndefined());
        promise->SetPrivate(currentContext, hooks->trigger_id_smb.Get(isolate),
                            Integer::New(isolate, 0));
      }
    } else if (type == PromiseHookType::kBefore) {
      AsyncContext ctx;
      ctx.execution_async_id =
          promise->GetPrivate(currentContext, hooks->async_id_smb.Get(isolate))
              .ToLocalChecked()
              .As<Integer>()
              ->Value();
      ctx.trigger_async_id =
          promise
              ->GetPrivate(currentContext, hooks->trigger_id_smb.Get(isolate))
              .ToLocalChecked()
              .As<Integer>()
              ->Value();
      hooks->asyncContexts.push(ctx);
    } else if (type == PromiseHookType::kAfter) {
      hooks->asyncContexts.pop();
    }
    if (!i::StackLimitCheck{i_isolate}.HasOverflowed()) {
      base::RecursiveMutexGuard lock_guard(&hooks->async_wraps_mutex_);
      for (const auto& wrap : hooks->async_wraps_) {
        PromiseHookDispatch(type, promise, parent, *wrap, hooks);
        if (try_catch.HasCaught()) break;
      }
      if (try_catch.HasCaught()) Shell::ReportException(isolate, &try_catch);
    }
  }
  if (!scheduled_exception.is_null()) {
    i_isolate->set_scheduled_exception(*scheduled_exception);
  }
}

void AsyncHooks::PromiseHookDispatch(PromiseHookType type,
                                     Local<Promise> promise,
                                     Local<Value> parent,
                                     const AsyncHooksWrap& wrap,
                                     AsyncHooks* hooks) {
  if (!wrap.IsEnabled()) return;
  v8::Isolate* v8_isolate = hooks->isolate_;
  HandleScope handle_scope(v8_isolate);

  Local<Value> rcv = Undefined(v8_isolate);
  Local<Context> context = v8_isolate->GetCurrentContext();
  Local<Value> async_id =
      promise->GetPrivate(context, hooks->async_id_smb.Get(v8_isolate))
          .ToLocalChecked();
  Local<Value> args[1] = {async_id};

  switch (type) {
    case PromiseHookType::kInit:
      if (!wrap.init_function().IsEmpty()) {
        Local<Value> initArgs[4] = {
            async_id, String::NewFromUtf8Literal(v8_isolate, "PROMISE"),
            promise->GetPrivate(context, hooks->trigger_id_smb.Get(v8_isolate))
                .ToLocalChecked(),
            promise};
        USE(wrap.init_function()->Call(context, rcv, 4, initArgs));
      }
      break;
    case PromiseHookType::kBefore:
      if (!wrap.before_function().IsEmpty()) {
        USE(wrap.before_function()->Call(context, rcv, 1, args));
      }
      break;
    case PromiseHookType::kAfter:
      if (!wrap.after_function().IsEmpty()) {
        USE(wrap.after_function()->Call(context, rcv, 1, args));
      }
      break;
    case PromiseHookType::kResolve:
      if (!wrap.promiseResolve_function().IsEmpty()) {
        USE(wrap.promiseResolve_function()->Call(context, rcv, 1, args));
      }
  }
}

}  // namespace v8
