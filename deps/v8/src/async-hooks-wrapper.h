// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ASYNC_HOOKS_WRAPPER_H_
#define V8_ASYNC_HOOKS_WRAPPER_H_

#include <stack>

#include "include/v8.h"
#include "src/objects.h"

namespace v8 {

typedef double async_id_t;

struct AsyncContext {
  async_id_t execution_async_id;
  async_id_t trigger_async_id;
};

class AsyncHooksWrap {
 public:
  explicit AsyncHooksWrap(Isolate* isolate) {
    enabled_ = false;
    isolate_ = isolate;
  }
  void Enable();
  void Disable();
  bool IsEnabled() const { return enabled_; }

  inline v8::Local<v8::Function> init_function() const;
  inline void set_init_function(v8::Local<v8::Function> value);
  inline v8::Local<v8::Function> before_function() const;
  inline void set_before_function(v8::Local<v8::Function> value);
  inline v8::Local<v8::Function> after_function() const;
  inline void set_after_function(v8::Local<v8::Function> value);
  inline v8::Local<v8::Function> promiseResolve_function() const;
  inline void set_promiseResolve_function(v8::Local<v8::Function> value);

 private:
  Isolate* isolate_;

  Persistent<v8::Function> init_function_;
  Persistent<v8::Function> before_function_;
  Persistent<v8::Function> after_function_;
  Persistent<v8::Function> promiseResolve_function_;

  bool enabled_;
};

class AsyncHooks {
 public:
  explicit AsyncHooks(Isolate* isolate) {
    isolate_ = isolate;

    AsyncContext ctx;
    ctx.execution_async_id = 1;
    ctx.trigger_async_id = 0;
    asyncContexts.push(ctx);
    current_async_id = 1;

    Initialize();
  }
  ~AsyncHooks() { Deinitialize(); }

  async_id_t GetExecutionAsyncId() const;
  async_id_t GetTriggerAsyncId() const;

  Local<Object> CreateHook(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  std::vector<AsyncHooksWrap*> async_wraps_;
  Isolate* isolate_;
  Persistent<FunctionTemplate> async_hook_ctor;
  Persistent<ObjectTemplate> async_hooks_templ;
  Persistent<Private> async_id_smb;
  Persistent<Private> trigger_id_smb;

  void Initialize();
  void Deinitialize();

  static void ShellPromiseHook(PromiseHookType type, Local<Promise> promise,
                               Local<Value> parent);
  static void PromiseHookDispatch(PromiseHookType type, Local<Promise> promise,
                                  Local<Value> parent, AsyncHooksWrap* wrap,
                                  AsyncHooks* hooks);

  std::stack<AsyncContext> asyncContexts;
  async_id_t current_async_id;
};

}  // namespace v8

#endif  // V8_ASYNC_HOOKS_WRAPPER_H_
