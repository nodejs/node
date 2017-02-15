#include "async-wrap.h"
#include "async-wrap-inl.h"
#include "env.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "util.h"
#include "util-inl.h"

#include <stdint.h>

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::Value;

const uint32_t kOnTimeout = 0;

class TimerWrap : public HandleWrap {
 public:
  static void Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context) {
    Environment* env = Environment::GetCurrent(context);
    Local<FunctionTemplate> constructor = env->NewFunctionTemplate(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Timer"));
    constructor->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "kOnTimeout"),
                     Integer::New(env->isolate(), kOnTimeout));

    env->SetTemplateMethod(constructor, "now", Now);

    env->SetProtoMethod(constructor, "close", HandleWrap::Close);
    env->SetProtoMethod(constructor, "ref", HandleWrap::Ref);
    env->SetProtoMethod(constructor, "unref", HandleWrap::Unref);
    env->SetProtoMethod(constructor, "hasRef", HandleWrap::HasRef);

    env->SetProtoMethod(constructor, "start", Start);
    env->SetProtoMethod(constructor, "stop", Stop);

    target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "Timer"),
                constructor->GetFunction());
  }

  size_t self_size() const override { return sizeof(*this); }

 private:
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
    wrap->MakeCallback(kOnTimeout, 0, nullptr);
  }

  static void Now(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    uv_update_time(env->event_loop());
    uint64_t now = uv_now(env->event_loop());
    CHECK(now >= env->timer_base());
    now -= env->timer_base();
    if (now <= 0xfffffff)
      args.GetReturnValue().Set(static_cast<uint32_t>(now));
    else
      args.GetReturnValue().Set(static_cast<double>(now));
  }

  uv_timer_t handle_;
};


}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(timer_wrap, node::TimerWrap::Initialize)
