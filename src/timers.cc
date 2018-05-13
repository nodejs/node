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

#include "node_internals.h"

#include <stdint.h>

namespace node {
namespace timers {

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::Value;

void SetupTimers(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsFunction());
  CHECK(args[1]->IsFunction());
  auto env = Environment::GetCurrent(args);

  env->set_immediate_callback_function(args[0].As<Function>());
  env->set_timers_callback_function(args[1].As<Function>());

  auto timer_start_cb = [] (const FunctionCallbackInfo<Value>& args) {
    auto env = Environment::GetCurrent(args);
    env->ScheduleTimer(args[0]->IntegerValue(env->context()).FromJust());
  };
  auto timer_start_function =
      env->NewFunctionTemplate(timer_start_cb)->GetFunction(env->context())
          .ToLocalChecked();

  auto timer_toggle_ref_cb = [] (const FunctionCallbackInfo<Value>& args) {
    Environment::GetCurrent(args)->ToggleTimerRef(args[0]->IsTrue());
  };
  auto timer_toggle_ref_function =
      env->NewFunctionTemplate(timer_toggle_ref_cb)->GetFunction(env->context())
          .ToLocalChecked();

  auto imm_toggle_ref_cb = [] (const FunctionCallbackInfo<Value>& args) {
    Environment::GetCurrent(args)->ToggleImmediateRef(args[0]->IsTrue());
  };
  auto imm_toggle_ref_function =
      env->NewFunctionTemplate(imm_toggle_ref_cb)->GetFunction(env->context())
          .ToLocalChecked();

  auto result = Array::New(env->isolate(), 4);
  result->Set(env->context(), 0, timer_start_function).FromJust();
  result->Set(env->context(), 1, timer_toggle_ref_function).FromJust();
  result->Set(env->context(), 2,
              env->immediate_info()->fields().GetJSArray()).FromJust();
  result->Set(env->context(), 3, imm_toggle_ref_function).FromJust();

  args.GetReturnValue().Set(result);
}

void GetLibuvNow(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  args.GetReturnValue().Set(env->GetNow());
}

void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "getLibuvNow", GetLibuvNow);
  env->SetMethod(target, "setupTimers", SetupTimers);
}


}  // namespace timers
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(timers, node::timers::Initialize)
