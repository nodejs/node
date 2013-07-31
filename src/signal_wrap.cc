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

static Cached<String> onsignal_sym;


class SignalWrap : public HandleWrap {
 public:
  static void Initialize(Handle<Object> target) {
    HandleScope scope(node_isolate);

    HandleWrap::Initialize(target);

    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("Signal"));

    NODE_SET_PROTOTYPE_METHOD(constructor, "close", HandleWrap::Close);
    NODE_SET_PROTOTYPE_METHOD(constructor, "ref", HandleWrap::Ref);
    NODE_SET_PROTOTYPE_METHOD(constructor, "unref", HandleWrap::Unref);
    NODE_SET_PROTOTYPE_METHOD(constructor, "start", Start);
    NODE_SET_PROTOTYPE_METHOD(constructor, "stop", Stop);

    onsignal_sym = String::New("onsignal");

    target->Set(String::NewSymbol("Signal"), constructor->GetFunction());
  }

 private:
  static void New(const FunctionCallbackInfo<Value>& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());

    HandleScope scope(node_isolate);
    new SignalWrap(args.This());
  }

  explicit SignalWrap(Handle<Object> object)
      : HandleWrap(object, reinterpret_cast<uv_handle_t*>(&handle_)) {
    int r = uv_signal_init(uv_default_loop(), &handle_);
    assert(r == 0);
  }

  ~SignalWrap() {
  }

  static void Start(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    UNWRAP(SignalWrap)

    int signum = args[0]->Int32Value();
    int err = uv_signal_start(&wrap->handle_, OnSignal, signum);
    args.GetReturnValue().Set(err);
  }

  static void Stop(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);
    UNWRAP(SignalWrap)

    int err = uv_signal_stop(&wrap->handle_);
    args.GetReturnValue().Set(err);
  }

  static void OnSignal(uv_signal_t* handle, int signum) {
    HandleScope scope(node_isolate);

    SignalWrap* wrap = container_of(handle, SignalWrap, handle_);
    assert(wrap);

    Local<Value> argv[1] = { Integer::New(signum, node_isolate) };
    MakeCallback(wrap->object(), onsignal_sym, ARRAY_SIZE(argv), argv);
  }

  uv_signal_t handle_;
};


}  // namespace node


NODE_MODULE(node_signal_wrap, node::SignalWrap::Initialize)
