// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "async_wrap-inl.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "util-inl.h"

#include <stdint.h>

namespace node {
namespace {

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

class TimerWrap : public HandleWrap {
 public:
  static void Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context) {
    Environment* env = Environment::GetCurrent(context);
    Local<FunctionTemplate> constructor = env->NewFunctionTemplate(New);
    Local<String> timerString = FIXED_ONE_BYTE_STRING(env->isolate(), "Timer");
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(timerString);

    env->SetTemplateMethod(constructor, "now", Now);

    AsyncWrap::AddWrapMethods(env, constructor);

    env->SetProtoMethod(constructor, "close", HandleWrap::Close);
    env->SetProtoMethod(constructor, "ref", HandleWrap::Ref);
    env->SetProtoMethod(constructor, "unref", HandleWrap::Unref);
    env->SetProtoMethod(constructor, "hasRef", HandleWrap::HasRef);

    env->SetProtoMethod(constructor, "start", Start);
    env->SetProtoMethod(constructor, "stop", Stop);

    target->Set(timerString, constructor->GetFunction());

    target->Set(env->context(),
                FIXED_ONE_BYTE_STRING(env->isolate(), "setupTimers"),
                env->NewFunctionTemplate(SetupTimers)
                   ->GetFunction(env->context()).ToLocalChecked()).FromJust();
  }

  size_t self_size() const override { return sizeof(*this); }

 private:
  static void SetupTimers(const FunctionCallbackInfo<Value>& args) {
    CHECK(args[0]->IsFunction());
    CHECK(args[1]->IsFunction());
    auto env = Environment::GetCurrent(args);

    env->set_immediate_callback_function(args[0].As<Function>());
    env->set_timers_callback_function(args[1].As<Function>());

    auto toggle_ref_cb = [] (const FunctionCallbackInfo<Value>& args) {
      Environment::GetCurrent(args)->ToggleImmediateRef(args[0]->IsTrue());
    };
    auto toggle_ref_function =
        env->NewFunctionTemplate(toggle_ref_cb)->GetFunction(env->context())
        .ToLocalChecked();
    auto result = Array::New(env->isolate(), 2);
    result->Set(env->context(), 0,
                env->immediate_info()->fields().GetJSArray()).FromJust();
    result->Set(env->context(), 1, toggle_ref_function).FromJust();
    args.GetReturnValue().Set(result);
  }

  static void New(const FunctionCallbackInfo<Value>& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    CHECK(args.IsConstructCall());
    Environment* env = Environment::GetCurrent(args);
    new TimerWrap(env, args.This());
  }

  TimerWrap(Environment* env, Local<Object> object)
      : HandleWrap(env,
                   object,
                   reinterpret_cast<uv_handle_t*>(&handle_),
                   AsyncWrap::PROVIDER_TIMERWRAP) {
    int r = uv_timer_init(env->event_loop(), &handle_);
    CHECK_EQ(r, 0);
  }

  static void Start(const FunctionCallbackInfo<Value>& args) {
    TimerWrap* wrap = Unwrap<TimerWrap>(args.Holder());

    CHECK(HandleWrap::IsAlive(wrap));

    int64_t timeout = args[0]->IntegerValue();
    int err = uv_timer_start(&wrap->handle_, OnTimeout, timeout, 0);
    args.GetReturnValue().Set(err);
  }

  static void Stop(const FunctionCallbackInfo<Value>& args) {
    TimerWrap* wrap = Unwrap<TimerWrap>(args.Holder());

    CHECK(HandleWrap::IsAlive(wrap));

    int err = uv_timer_stop(&wrap->handle_);
    args.GetReturnValue().Set(err);
  }

  static void OnTimeout(uv_timer_t* handle) {
    TimerWrap* wrap = static_cast<TimerWrap*>(handle->data);
    Environment* env = wrap->env();
    HandleScope handle_scope(env->isolate());
    Context::Scope context_scope(env->context());
    Local<Value> ret;
    Local<Value> args[1];
    do {
      args[0] = env->GetNow();
      ret = wrap->MakeCallback(env->timers_callback_function(), 1, args)
                .ToLocalChecked();
    } while (ret->IsUndefined() &&
             !env->tick_info()->has_thrown() &&
             wrap->object()->Get(env->context(),
                                 env->owner_string()).ToLocalChecked()
                                                     ->IsUndefined());
  }

  static void Now(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    args.GetReturnValue().Set(env->GetNow());
  }

  uv_timer_t handle_;
};


}  // anonymous namespace
}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(timer_wrap, node::TimerWrap::Initialize)
