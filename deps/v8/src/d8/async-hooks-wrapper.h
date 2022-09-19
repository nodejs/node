// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_D8_ASYNC_HOOKS_WRAPPER_H_
#define V8_D8_ASYNC_HOOKS_WRAPPER_H_

#include <stack>

#include "include/v8-function-callback.h"
#include "include/v8-local-handle.h"
#include "include/v8-promise.h"
#include "src/objects/objects.h"

namespace v8 {

class Function;
class Isolate;
class ObjectTemplate;
class Value;

using async_id_t = double;

struct AsyncContext {
  async_id_t execution_async_id;
  async_id_t trigger_async_id;
};

class AsyncHooksWrap {
 public:
  explicit AsyncHooksWrap(Isolate* isolate)
      : isolate_(isolate), enabled_(false) {}
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
  explicit AsyncHooks(Isolate* isolate);
  ~AsyncHooks();

  async_id_t GetExecutionAsyncId() const;
  async_id_t GetTriggerAsyncId() const;

  Local<Object> CreateHook(const v8::FunctionCallbackInfo<v8::Value>& args);

  Persistent<FunctionTemplate> async_hook_ctor;

 private:
  base::RecursiveMutex async_wraps_mutex_;
  std::vector<std::shared_ptr<AsyncHooksWrap>> async_wraps_;
  Isolate* isolate_;
  Persistent<ObjectTemplate> async_hooks_templ;
  Persistent<Private> async_id_smb;
  Persistent<Private> trigger_id_smb;

  static void ShellPromiseHook(PromiseHookType type, Local<Promise> promise,
                               Local<Value> parent);
  static void PromiseHookDispatch(PromiseHookType type, Local<Promise> promise,
                                  Local<Value> parent,
                                  const AsyncHooksWrap& wrap,
                                  AsyncHooks* hooks);

  std::stack<AsyncContext> asyncContexts;
  async_id_t current_async_id;
};

}  // namespace v8

#endif  // V8_D8_ASYNC_HOOKS_WRAPPER_H_
