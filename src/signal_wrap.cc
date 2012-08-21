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

static Persistent<String> onsignal_sym;


class SignalWrap : public HandleWrap {
 public:
  static void Initialize(Handle<Object> target) {
    HandleScope scope;

    HandleWrap::Initialize(target);

    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("Signal"));

    NODE_SET_PROTOTYPE_METHOD(constructor, "close", HandleWrap::Close);
    NODE_SET_PROTOTYPE_METHOD(constructor, "ref", HandleWrap::Ref);
    NODE_SET_PROTOTYPE_METHOD(constructor, "unref", HandleWrap::Unref);
    NODE_SET_PROTOTYPE_METHOD(constructor, "start", Start);
    NODE_SET_PROTOTYPE_METHOD(constructor, "stop", Stop);

    onsignal_sym = NODE_PSYMBOL("onsignal");

    target->Set(String::NewSymbol("Signal"), constructor->GetFunction());
  }

 private:
  static Handle<Value> New(const Arguments& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());

    HandleScope scope;
    new SignalWrap(args.This());

    return scope.Close(args.This());
  }

  SignalWrap(Handle<Object> object)
      : HandleWrap(object, reinterpret_cast<uv_handle_t*>(&handle_)) {
    int r = uv_signal_init(uv_default_loop(), &handle_);
    assert(r == 0);
  }

  ~SignalWrap() {
  }

  static Handle<Value> Start(const Arguments& args) {
    HandleScope scope;

    UNWRAP(SignalWrap)

    int signum = args[0]->Int32Value();

    int r = uv_signal_start(&wrap->handle_, OnSignal, signum);

    if (r) SetErrno(uv_last_error(uv_default_loop()));

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Stop(const Arguments& args) {
    HandleScope scope;

    UNWRAP(SignalWrap)

    int r = uv_signal_stop(&wrap->handle_);

    if (r) SetErrno(uv_last_error(uv_default_loop()));

    return scope.Close(Integer::New(r));
  }

  static void OnSignal(uv_signal_t* handle, int signum) {
    HandleScope scope;

    SignalWrap* wrap = container_of(handle, SignalWrap, handle_);
    assert(wrap);

    Local<Value> argv[1] = { Integer::New(signum) };
    MakeCallback(wrap->object_, onsignal_sym, ARRAY_SIZE(argv), argv);
  }

  uv_signal_t handle_;
};


}  // namespace node


NODE_MODULE(node_signal_wrap, node::SignalWrap::Initialize)
