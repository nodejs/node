// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/async-hooks-wrapper.h"
#include "src/d8.h"

namespace v8 {

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

static AsyncHooksWrap* UnwrapHook(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  Local<Object> hook = args.This();
  Local<External> wrap = Local<External>::Cast(hook->GetInternalField(0));
  void* ptr = wrap->Value();
  return static_cast<AsyncHooksWrap*>(ptr);
}

static void EnableHook(const v8::FunctionCallbackInfo<v8::Value>& args) {
  AsyncHooksWrap* wrap = UnwrapHook(args);
  wrap->Enable();
}

static void DisableHook(const v8::FunctionCallbackInfo<v8::Value>& args) {
  AsyncHooksWrap* wrap = UnwrapHook(args);
  wrap->Disable();
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
    isolate->ThrowException(
        String::NewFromUtf8(isolate, "Invalid arguments passed to createHook",
                            NewStringType::kNormal)
            .ToLocalChecked());
    return Local<Object>();
  }

  AsyncHooksWrap* wrap = new AsyncHooksWrap(isolate);

  Local<Object> fn_obj = args[0].As<Object>();

#define SET_HOOK_FN(name)                                                   \
  Local<Value> name##_v =                                                   \
      fn_obj                                                                \
          ->Get(currentContext,                                             \
                String::NewFromUtf8(isolate, #name, NewStringType::kNormal) \
                    .ToLocalChecked())                                      \
          .ToLocalChecked();                                                \
  if (name##_v->IsFunction()) {                                             \
    wrap->set_##name##_function(name##_v.As<Function>());                   \
  }

  SET_HOOK_FN(init);
  SET_HOOK_FN(before);
  SET_HOOK_FN(after);
  SET_HOOK_FN(promiseResolve);
#undef SET_HOOK_FN

  async_wraps_.push_back(wrap);

  Local<Object> obj = async_hooks_templ.Get(isolate)
                          ->NewInstance(currentContext)
                          .ToLocalChecked();
  obj->SetInternalField(0, External::New(isolate, wrap));

  return handle_scope.Escape(obj);
}

void AsyncHooks::ShellPromiseHook(PromiseHookType type, Local<Promise> promise,
                                  Local<Value> parent) {
  AsyncHooks* hooks =
      PerIsolateData::Get(promise->GetIsolate())->GetAsyncHooks();

  HandleScope handle_scope(hooks->isolate_);

  Local<Context> currentContext = hooks->isolate_->GetCurrentContext();

  if (type == PromiseHookType::kInit) {
    ++hooks->current_async_id;
    Local<Integer> async_id =
        Integer::New(hooks->isolate_, hooks->current_async_id);

    promise->SetPrivate(currentContext,
                        hooks->async_id_smb.Get(hooks->isolate_), async_id);
    if (parent->IsPromise()) {
      Local<Promise> parent_promise = parent.As<Promise>();
      Local<Value> parent_async_id =
          parent_promise
              ->GetPrivate(hooks->isolate_->GetCurrentContext(),
                           hooks->async_id_smb.Get(hooks->isolate_))
              .ToLocalChecked();
      promise->SetPrivate(currentContext,
                          hooks->trigger_id_smb.Get(hooks->isolate_),
                          parent_async_id);
    } else {
      CHECK(parent->IsUndefined());
      Local<Integer> trigger_id = Integer::New(hooks->isolate_, 0);
      promise->SetPrivate(currentContext,
                          hooks->trigger_id_smb.Get(hooks->isolate_),
                          trigger_id);
    }
  } else if (type == PromiseHookType::kBefore) {
    AsyncContext ctx;
    ctx.execution_async_id =
        promise
            ->GetPrivate(hooks->isolate_->GetCurrentContext(),
                         hooks->async_id_smb.Get(hooks->isolate_))
            .ToLocalChecked()
            .As<Integer>()
            ->Value();
    ctx.trigger_async_id =
        promise
            ->GetPrivate(hooks->isolate_->GetCurrentContext(),
                         hooks->trigger_id_smb.Get(hooks->isolate_))
            .ToLocalChecked()
            .As<Integer>()
            ->Value();
    hooks->asyncContexts.push(ctx);
  } else if (type == PromiseHookType::kAfter) {
    hooks->asyncContexts.pop();
  }

  for (AsyncHooksWrap* wrap : hooks->async_wraps_) {
    PromiseHookDispatch(type, promise, parent, wrap, hooks);
  }
}

void AsyncHooks::Initialize() {
  HandleScope handle_scope(isolate_);

  async_hook_ctor.Reset(isolate_, FunctionTemplate::New(isolate_));
  async_hook_ctor.Get(isolate_)->SetClassName(
      String::NewFromUtf8(isolate_, "AsyncHook", NewStringType::kNormal)
          .ToLocalChecked());

  async_hooks_templ.Reset(isolate_,
                          async_hook_ctor.Get(isolate_)->InstanceTemplate());
  async_hooks_templ.Get(isolate_)->SetInternalFieldCount(1);
  async_hooks_templ.Get(isolate_)->Set(
      String::NewFromUtf8(isolate_, "enable"),
      FunctionTemplate::New(isolate_, EnableHook));
  async_hooks_templ.Get(isolate_)->Set(
      String::NewFromUtf8(isolate_, "disable"),
      FunctionTemplate::New(isolate_, DisableHook));

  async_id_smb.Reset(isolate_, Private::New(isolate_));
  trigger_id_smb.Reset(isolate_, Private::New(isolate_));

  isolate_->SetPromiseHook(ShellPromiseHook);
}

void AsyncHooks::Deinitialize() {
  isolate_->SetPromiseHook(nullptr);
  for (AsyncHooksWrap* wrap : async_wraps_) {
    delete wrap;
  }
}

void AsyncHooks::PromiseHookDispatch(PromiseHookType type,
                                     Local<Promise> promise,
                                     Local<Value> parent, AsyncHooksWrap* wrap,
                                     AsyncHooks* hooks) {
  if (!wrap->IsEnabled()) {
    return;
  }

  HandleScope handle_scope(hooks->isolate_);

  TryCatch try_catch(hooks->isolate_);
  try_catch.SetVerbose(true);

  Local<Value> rcv = Undefined(hooks->isolate_);
  Local<Value> async_id =
      promise
          ->GetPrivate(hooks->isolate_->GetCurrentContext(),
                       hooks->async_id_smb.Get(hooks->isolate_))
          .ToLocalChecked();
  Local<Value> args[1] = {async_id};

  // Sacrifice the brevity for readability and debugfulness
  if (type == PromiseHookType::kInit) {
    if (!wrap->init_function().IsEmpty()) {
      Local<Value> initArgs[4] = {
          async_id,
          String::NewFromUtf8(hooks->isolate_, "PROMISE",
                              NewStringType::kNormal)
              .ToLocalChecked(),
          promise
              ->GetPrivate(hooks->isolate_->GetCurrentContext(),
                           hooks->trigger_id_smb.Get(hooks->isolate_))
              .ToLocalChecked(),
          promise};
      wrap->init_function()->Call(rcv, 4, initArgs);
    }
  } else if (type == PromiseHookType::kBefore) {
    if (!wrap->before_function().IsEmpty()) {
      wrap->before_function()->Call(rcv, 1, args);
    }
  } else if (type == PromiseHookType::kAfter) {
    if (!wrap->after_function().IsEmpty()) {
      wrap->after_function()->Call(rcv, 1, args);
    }
  } else if (type == PromiseHookType::kResolve) {
    if (!wrap->promiseResolve_function().IsEmpty()) {
      wrap->promiseResolve_function()->Call(rcv, 1, args);
    }
  }

  if (try_catch.HasCaught()) {
    Shell::ReportException(hooks->isolate_, &try_catch);
  }
}

}  // namespace v8
