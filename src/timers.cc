#include "env-inl.h"
#include "util-inl.h"
#include "v8.h"

#include <cstdint>

namespace node {
namespace {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

void SetupTimers(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsFunction());
  CHECK(args[1]->IsFunction());
  auto env = Environment::GetCurrent(args);

  env->set_immediate_callback_function(args[0].As<Function>());
  env->set_timers_callback_function(args[1].As<Function>());
}

void GetLibuvNow(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  args.GetReturnValue().Set(env->GetNow());
}

void ScheduleTimer(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  env->ScheduleTimer(args[0]->IntegerValue(env->context()).FromJust());
}

void ToggleTimerRef(const FunctionCallbackInfo<Value>& args) {
  Environment::GetCurrent(args)->ToggleTimerRef(args[0]->IsTrue());
}

void ToggleImmediateRef(const FunctionCallbackInfo<Value>& args) {
  Environment::GetCurrent(args)->ToggleImmediateRef(args[0]->IsTrue());
}

void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "getLibuvNow", GetLibuvNow);
  env->SetMethod(target, "setupTimers", SetupTimers);
  env->SetMethod(target, "scheduleTimer", ScheduleTimer);
  env->SetMethod(target, "toggleTimerRef", ToggleTimerRef);
  env->SetMethod(target, "toggleImmediateRef", ToggleImmediateRef);

  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "immediateInfo"),
              env->immediate_info()->fields().GetJSArray()).Check();
}


}  // anonymous namespace
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(timers, node::Initialize)
