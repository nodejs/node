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

#include "env.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

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

class SignalWrap : public HandleWrap {
 public:
  static void Initialize(Handle<Object> target,
                         Handle<Value> unused,
                         Handle<Context> context) {
    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(FIXED_ONE_BYTE_STRING(node_isolate, "Signal"));

    NODE_SET_PROTOTYPE_METHOD(constructor, "close", HandleWrap::Close);
    NODE_SET_PROTOTYPE_METHOD(constructor, "ref", HandleWrap::Ref);
    NODE_SET_PROTOTYPE_METHOD(constructor, "unref", HandleWrap::Unref);
    NODE_SET_PROTOTYPE_METHOD(constructor, "start", Start);
    NODE_SET_PROTOTYPE_METHOD(constructor, "stop", Stop);

    target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "Signal"),
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
    new SignalWrap(env, args.This());
  }

  SignalWrap(Environment* env, Handle<Object> object)
      : HandleWrap(env, object, reinterpret_cast<uv_handle_t*>(&handle_)) {
    int r = uv_signal_init(env->event_loop(), &handle_);
    assert(r == 0);
  }

  ~SignalWrap() {
  }

  static void Start(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    SignalWrap* wrap = Unwrap<SignalWrap>(args.This());

    int signum = args[0]->Int32Value();
    int err = uv_signal_start(&wrap->handle_, OnSignal, signum);
    args.GetReturnValue().Set(err);
  }

  static void Stop(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    SignalWrap* wrap = Unwrap<SignalWrap>(args.This());

    int err = uv_signal_stop(&wrap->handle_);
    args.GetReturnValue().Set(err);
  }

  static void OnSignal(uv_signal_t* handle, int signum) {
    SignalWrap* wrap = container_of(handle, SignalWrap, handle_);
    Environment* env = wrap->env();
    Context::Scope context_scope(env->context());
    HandleScope handle_scope(env->isolate());
    Local<Value> arg = Integer::New(signum, env->isolate());
    MakeCallback(env, wrap->object(), env->onsignal_string(), 1, &arg);
  }

  uv_signal_t handle_;
};


}  // namespace node


NODE_MODULE_CONTEXT_AWARE(node_signal_wrap, node::SignalWrap::Initialize)
