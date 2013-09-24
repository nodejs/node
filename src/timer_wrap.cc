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
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::Value;

const uint32_t kOnTimeout = 0;

class TimerWrap : public HandleWrap {
 public:
  static void Initialize(Handle<Object> target,
                         Handle<Value> unused,
                         Handle<Context> context) {
    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(FIXED_ONE_BYTE_STRING(node_isolate, "Timer"));
    constructor->Set(FIXED_ONE_BYTE_STRING(node_isolate, "kOnTimeout"),
                     Integer::New(kOnTimeout, node_isolate));

    NODE_SET_METHOD(constructor, "now", Now);

    NODE_SET_PROTOTYPE_METHOD(constructor, "close", HandleWrap::Close);
    NODE_SET_PROTOTYPE_METHOD(constructor, "ref", HandleWrap::Ref);
    NODE_SET_PROTOTYPE_METHOD(constructor, "unref", HandleWrap::Unref);

    NODE_SET_PROTOTYPE_METHOD(constructor, "start", Start);
    NODE_SET_PROTOTYPE_METHOD(constructor, "stop", Stop);
    NODE_SET_PROTOTYPE_METHOD(constructor, "setRepeat", SetRepeat);
    NODE_SET_PROTOTYPE_METHOD(constructor, "getRepeat", GetRepeat);
    NODE_SET_PROTOTYPE_METHOD(constructor, "again", Again);

    target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "Timer"),
                constructor->GetFunction());
  }

 private:
  static void New(const FunctionCallbackInfo<Value>& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());
    Environment* env = Environment::GetCurrent(args.GetIsolate());
    HandleScope handle_scope(args.GetIsolate());
    new TimerWrap(env, args.This());
  }

  TimerWrap(Environment* env, Handle<Object> object)
      : HandleWrap(env, object, reinterpret_cast<uv_handle_t*>(&handle_)) {
    int r = uv_timer_init(env->event_loop(), &handle_);
    assert(r == 0);
  }

  ~TimerWrap() {
  }

  static void Start(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    TimerWrap* wrap = Unwrap<TimerWrap>(args.This());

    int64_t timeout = args[0]->IntegerValue();
    int64_t repeat = args[1]->IntegerValue();
    int err = uv_timer_start(&wrap->handle_, OnTimeout, timeout, repeat);
    args.GetReturnValue().Set(err);
  }

  static void Stop(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    TimerWrap* wrap = Unwrap<TimerWrap>(args.This());

    int err = uv_timer_stop(&wrap->handle_);
    args.GetReturnValue().Set(err);
  }

  static void Again(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    TimerWrap* wrap = Unwrap<TimerWrap>(args.This());

    int err = uv_timer_again(&wrap->handle_);
    args.GetReturnValue().Set(err);
  }

  static void SetRepeat(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    TimerWrap* wrap = Unwrap<TimerWrap>(args.This());

    int64_t repeat = args[0]->IntegerValue();
    uv_timer_set_repeat(&wrap->handle_, repeat);
    args.GetReturnValue().Set(0);
  }

  static void GetRepeat(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    TimerWrap* wrap = Unwrap<TimerWrap>(args.This());

    int64_t repeat = uv_timer_get_repeat(&wrap->handle_);
    args.GetReturnValue().Set(static_cast<double>(repeat));
  }

  static void OnTimeout(uv_timer_t* handle, int status) {
    TimerWrap* wrap = static_cast<TimerWrap*>(handle->data);
    Environment* env = wrap->env();
    Context::Scope context_scope(env->context());
    HandleScope handle_scope(env->isolate());
    Local<Value> argv[1] = { Integer::New(status, node_isolate) };
    wrap->MakeCallback(kOnTimeout, ARRAY_SIZE(argv), argv);
  }

  static void Now(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args.GetIsolate());
    HandleScope handle_scope(args.GetIsolate());
    uv_update_time(env->event_loop());
    double now = static_cast<double>(uv_now(env->event_loop()));
    args.GetReturnValue().Set(now);
  }

  uv_timer_t handle_;
};


}  // namespace node

NODE_MODULE_CONTEXT_AWARE(node_timer_wrap, node::TimerWrap::Initialize)
