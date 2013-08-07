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

#include "node.h"
#include "handle_wrap.h"

namespace node {

using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

static Cached<String> ontimeout_sym;

class TimerWrap : public HandleWrap {
 public:
  static void Initialize(Handle<Object> target) {
    HandleScope scope(node_isolate);

    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("Timer"));

    NODE_SET_METHOD(constructor, "now", Now);

    NODE_SET_PROTOTYPE_METHOD(constructor, "close", HandleWrap::Close);
    NODE_SET_PROTOTYPE_METHOD(constructor, "ref", HandleWrap::Ref);
    NODE_SET_PROTOTYPE_METHOD(constructor, "unref", HandleWrap::Unref);

    NODE_SET_PROTOTYPE_METHOD(constructor, "start", Start);
    NODE_SET_PROTOTYPE_METHOD(constructor, "stop", Stop);
    NODE_SET_PROTOTYPE_METHOD(constructor, "setRepeat", SetRepeat);
    NODE_SET_PROTOTYPE_METHOD(constructor, "getRepeat", GetRepeat);
    NODE_SET_PROTOTYPE_METHOD(constructor, "again", Again);

    ontimeout_sym = String::New("ontimeout");

    target->Set(String::NewSymbol("Timer"), constructor->GetFunction());
  }

 private:
  static void New(const FunctionCallbackInfo<Value>& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());
    HandleScope scope(node_isolate);
    new TimerWrap(args.This());
  }

  explicit TimerWrap(Handle<Object> object)
      : HandleWrap(object, reinterpret_cast<uv_handle_t*>(&handle_)) {
    int r = uv_timer_init(uv_default_loop(), &handle_);
    assert(r == 0);
  }

  ~TimerWrap() {
  }

  static void Start(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    UNWRAP(TimerWrap)

    int64_t timeout = args[0]->IntegerValue();
    int64_t repeat = args[1]->IntegerValue();
    int err = uv_timer_start(&wrap->handle_, OnTimeout, timeout, repeat);
    args.GetReturnValue().Set(err);
  }

  static void Stop(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    UNWRAP(TimerWrap)

    int err = uv_timer_stop(&wrap->handle_);
    args.GetReturnValue().Set(err);
  }

  static void Again(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    UNWRAP(TimerWrap)

    int err = uv_timer_again(&wrap->handle_);
    args.GetReturnValue().Set(err);
  }

  static void SetRepeat(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    UNWRAP(TimerWrap)

    int64_t repeat = args[0]->IntegerValue();
    uv_timer_set_repeat(&wrap->handle_, repeat);
    args.GetReturnValue().Set(0);
  }

  static void GetRepeat(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    UNWRAP(TimerWrap)

    int64_t repeat = uv_timer_get_repeat(&wrap->handle_);
    args.GetReturnValue().Set(static_cast<double>(repeat));
  }

  static void OnTimeout(uv_timer_t* handle, int status) {
    HandleScope scope(node_isolate);

    TimerWrap* wrap = static_cast<TimerWrap*>(handle->data);
    assert(wrap);

    Local<Value> argv[1] = { Integer::New(status, node_isolate) };
    MakeCallback(wrap->object(), ontimeout_sym, ARRAY_SIZE(argv), argv);
  }

  static void Now(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    double now = static_cast<double>(uv_now(uv_default_loop()));
    args.GetReturnValue().Set(now);
  }

  uv_timer_t handle_;
};


}  // namespace node

NODE_MODULE(node_timer_wrap, node::TimerWrap::Initialize)
