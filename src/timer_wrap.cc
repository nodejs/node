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

#include <node.h>
#include <handle_wrap.h>

#define UNWRAP \
  assert(!args.Holder().IsEmpty()); \
  assert(args.Holder()->InternalFieldCount() > 0); \
  TimerWrap* wrap =  \
      static_cast<TimerWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
  if (!wrap) { \
    uv_err_t err; \
    err.code = UV_EBADF; \
    SetErrno(err); \
    return scope.Close(Integer::New(-1)); \
  }

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


class TimerWrap : public HandleWrap {
 public:
  static void Initialize(Handle<Object> target) {
    HandleScope scope;

    HandleWrap::Initialize(target);

    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("Timer"));

    NODE_SET_PROTOTYPE_METHOD(constructor, "close", HandleWrap::Close);

    NODE_SET_PROTOTYPE_METHOD(constructor, "start", Start);
    NODE_SET_PROTOTYPE_METHOD(constructor, "stop", Stop);
    NODE_SET_PROTOTYPE_METHOD(constructor, "setRepeat", SetRepeat);
    NODE_SET_PROTOTYPE_METHOD(constructor, "getRepeat", GetRepeat);
    NODE_SET_PROTOTYPE_METHOD(constructor, "again", Again);

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
    active_ = false;

    int r = uv_timer_init(uv_default_loop(), &handle_);
    assert(r == 0);

    handle_.data = this;

    // uv_timer_init adds a loop reference. (That is, it calls uv_ref.) This
    // is not the behavior we want in Node. Timers should not increase the
    // ref count of the loop except when active.
    uv_unref(uv_default_loop());
  }

  ~TimerWrap() {
    if (!active_) uv_ref(uv_default_loop());
  }

  void StateChange() {
    bool was_active = active_;
    active_ = uv_is_active((uv_handle_t*) &handle_);

    if (!was_active && active_) {
      // If our state is changing from inactive to active, we
      // increase the loop's reference count.
      uv_ref(uv_default_loop());
    } else if (was_active && !active_) {
      // If our state is changing from active to inactive, we
      // decrease the loop's reference count.
      uv_unref(uv_default_loop());
    }
  }

  static Handle<Value> Start(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int64_t timeout = args[0]->IntegerValue();
    int64_t repeat = args[1]->IntegerValue();

    int r = uv_timer_start(&wrap->handle_, OnTimeout, timeout, repeat);

    // Error starting the timer.
    if (r) SetErrno(uv_last_error(uv_default_loop()));

    wrap->StateChange();

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Stop(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_timer_stop(&wrap->handle_);

    if (r) SetErrno(uv_last_error(uv_default_loop()));

    wrap->StateChange();

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Again(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_timer_again(&wrap->handle_);

    if (r) SetErrno(uv_last_error(uv_default_loop()));

    wrap->StateChange();

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> SetRepeat(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int64_t repeat = args[0]->IntegerValue();

    uv_timer_set_repeat(&wrap->handle_, repeat);

    return scope.Close(Integer::New(0));
  }

  static Handle<Value> GetRepeat(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int64_t repeat = uv_timer_get_repeat(&wrap->handle_);

    if (repeat < 0) SetErrno(uv_last_error(uv_default_loop()));

    return scope.Close(Integer::New(repeat));
  }

  static void OnTimeout(uv_timer_t* handle, int status) {
    HandleScope scope;

    TimerWrap* wrap = static_cast<TimerWrap*>(handle->data);
    assert(wrap);

    wrap->StateChange();

    Local<Value> argv[1] = { Integer::New(status) };
    MakeCallback(wrap->object_, "ontimeout", 1, argv);
  }

  uv_timer_t handle_;
  // This member is set false initially. When the timer is turned
  // on uv_ref is called. When the timer is turned off uv_unref is
  // called. Used to mirror libev semantics.
  bool active_;
};


}  // namespace node

NODE_MODULE(node_timer_wrap, node::TimerWrap::Initialize)
