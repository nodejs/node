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

using v8::Object;
using v8::Handle;
using v8::Local;
using v8::Persistent;
using v8::Value;
using v8::HandleScope;
using v8::FunctionTemplate;
using v8::String;
using v8::Function;
using v8::TryCatch;
using v8::Context;
using v8::Arguments;
using v8::Integer;

static Persistent<String> ontimeout_sym;

class TimerWrap : public HandleWrap {
 public:
  static void Initialize(Handle<Object> target) {
    HandleScope scope;

    HandleWrap::Initialize(target);

    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("Timer"));

    NODE_SET_PROTOTYPE_METHOD(constructor, "close", HandleWrap::Close);
    NODE_SET_PROTOTYPE_METHOD(constructor, "ref", HandleWrap::Ref);
    NODE_SET_PROTOTYPE_METHOD(constructor, "unref", HandleWrap::Unref);

    NODE_SET_PROTOTYPE_METHOD(constructor, "start", Start);
    NODE_SET_PROTOTYPE_METHOD(constructor, "stop", Stop);
    NODE_SET_PROTOTYPE_METHOD(constructor, "setRepeat", SetRepeat);
    NODE_SET_PROTOTYPE_METHOD(constructor, "getRepeat", GetRepeat);
    NODE_SET_PROTOTYPE_METHOD(constructor, "again", Again);

    ontimeout_sym = NODE_PSYMBOL("ontimeout");

    target->Set(String::NewSymbol("Timer"), constructor->GetFunction());
  }

 private:
  static Handle<Value> New(const Arguments& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());

    HandleScope scope;
    TimerWrap *wrap = new TimerWrap(args.This());
    assert(wrap);

    return scope.Close(args.This());
  }

  TimerWrap(Handle<Object> object)
      : HandleWrap(object, (uv_handle_t*) &handle_) {
    int r = uv_timer_init(uv_default_loop(), &handle_);
    assert(r == 0);
    handle_.data = this;
  }

  ~TimerWrap() {
  }

  static Handle<Value> Start(const Arguments& args) {
    HandleScope scope;

    UNWRAP(TimerWrap)

    int64_t timeout = args[0]->IntegerValue();
    int64_t repeat = args[1]->IntegerValue();

    int r = uv_timer_start(&wrap->handle_, OnTimeout, timeout, repeat);

    if (r) SetErrno(uv_last_error(uv_default_loop()));

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Stop(const Arguments& args) {
    HandleScope scope;

    UNWRAP(TimerWrap)

    int r = uv_timer_stop(&wrap->handle_);

    if (r) SetErrno(uv_last_error(uv_default_loop()));

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Again(const Arguments& args) {
    HandleScope scope;

    UNWRAP(TimerWrap)

    int r = uv_timer_again(&wrap->handle_);

    if (r) SetErrno(uv_last_error(uv_default_loop()));

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> SetRepeat(const Arguments& args) {
    HandleScope scope;

    UNWRAP(TimerWrap)

    int64_t repeat = args[0]->IntegerValue();

    uv_timer_set_repeat(&wrap->handle_, repeat);

    return scope.Close(Integer::New(0));
  }

  static Handle<Value> GetRepeat(const Arguments& args) {
    HandleScope scope;

    UNWRAP(TimerWrap)

    int64_t repeat = uv_timer_get_repeat(&wrap->handle_);

    if (repeat < 0) SetErrno(uv_last_error(uv_default_loop()));

    return scope.Close(Integer::New(repeat));
  }

  static void OnTimeout(uv_timer_t* handle, int status) {
    HandleScope scope;

    TimerWrap* wrap = static_cast<TimerWrap*>(handle->data);
    assert(wrap);

    Local<Value> argv[1] = { Integer::New(status) };
    MakeCallback(wrap->object_, ontimeout_sym, ARRAY_SIZE(argv), argv);
  }

  uv_timer_t handle_;
};


}  // namespace node

NODE_MODULE(node_timer_wrap, node::TimerWrap::Initialize)
