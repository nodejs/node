#include <v8.h>
#include <uv.h>
#include <node.h>

// Rules:
//
// - Do not throw from handle methods. Set errno.
//
// - MakeCallback may only be made directly off the event loop.
//   That is there can be no JavaScript stack frames underneith it.
//   (Is there anyway to assert that?)
//
// - No use of v8::WeakReferenceCallback. The close callback signifies that
//   we're done with a handle - external resources can be freed.
//
// - Reusable?


// Target API:
//
//  var socket = new Timer2();
//  socket.init();
//
//  var req = new Req();
//  req.init(socket);
//  req.onclose = function(status) {
//    if (s) {
//      console.error("failed to connect: %s", errno);
//    } else {
//      console.error("connected!");
//    }
//  })
//
//  var r = socket.connect(req, "127.0.0.1", 8000);
//
//  if (r) {
//    console.error("got error: %s", errno);
//  }


#define UNWRAP \
  assert(!args.Holder().IsEmpty()); \
  assert(args.Holder()->InternalFieldCount() > 0); \
  TimerWrap* wrap =  \
      static_cast<TimerWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
  if (!wrap) { \
    SetErrno(UV_EBADF); \
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

// MakeCallback may only be made directly off the event loop.
// That is there can be no JavaScript stack frames underneath it.
// (Is there any way to assert that?)
//
// Maybe make this a method of a node::Handle super class
//
// TODO: share me!
static void MakeCallback(Handle<Object> object,
                         const char* method,
                         int argc,
                         Handle<Value> argv[]) {
  HandleScope scope;

  Local<Value> callback_v = object->Get(String::New(method)); 
  assert(callback_v->IsFunction());
  Local<Function> callback = Local<Function>::Cast(callback_v);

  // TODO Hook for long stack traces to be made here.

  TryCatch try_catch;

  callback->Call(object, argc, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


// TODO: share me!
static void SetErrno(uv_err_code code) {
  uv_err_t err;
  err.code = code;
  Context::GetCurrent()->Global()->Set(String::NewSymbol("errno"),
                                       String::NewSymbol(uv_err_name(err)));
}


class TimerWrap {
 public:
  static void Initialize(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("Timer"));

    NODE_SET_PROTOTYPE_METHOD(constructor, "start", Start);
    NODE_SET_PROTOTYPE_METHOD(constructor, "stop", Stop);
    NODE_SET_PROTOTYPE_METHOD(constructor, "setRepeat", SetRepeat);
    NODE_SET_PROTOTYPE_METHOD(constructor, "getRepeat", GetRepeat);
    NODE_SET_PROTOTYPE_METHOD(constructor, "again", Again);
    NODE_SET_PROTOTYPE_METHOD(constructor, "close", Close);

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

  TimerWrap(Handle<Object> object) {
    int r = uv_timer_init(&handle_, OnClose, this);
    assert(r == 0); // How do we proxy this error up to javascript?
                    // Suggestion: uv_timer_init() returns void.
    assert(object_.IsEmpty());
    assert(object->InternalFieldCount() > 0);
    object_ = v8::Persistent<v8::Object>::New(object);
    object_->SetPointerInInternalField(0, this);
  }

  ~TimerWrap() {
    assert(!object_.IsEmpty());
    object_->SetPointerInInternalField(0, NULL);
    object_.Dispose();
  }

  static Handle<Value> Start(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int64_t timeout = args[0]->IntegerValue();
    int64_t repeat = args[1]->IntegerValue();

    int r = uv_timer_start(&wrap->handle_, OnTimeout, timeout, repeat);
    // Can r ever be an error?

    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Stop(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_timer_stop(&wrap->handle_);
    // Can r ever be an error?

    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Again(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_timer_again(&wrap->handle_);

    if (r) SetErrno(uv_last_error().code);

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

    if (repeat < 0) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(repeat));
  }

  // TODO: share me?
  static Handle<Value> Close(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_close(&wrap->handle_);

    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  static void OnTimeout(uv_handle_t* handle, int status) {
    HandleScope scope;
    TimerWrap* wrap = static_cast<TimerWrap*>(handle->data);
    assert(wrap);
    Local<Value> argv[1] = { Integer::New(status) };
    MakeCallback(wrap->object_, "ontimeout", 1, argv);
  }

  static void OnClose(uv_handle_t* handle, int status) {
    HandleScope scope;

    TimerWrap* wrap = static_cast<TimerWrap*>(handle->data);
    Local<Value> argv[1] = { Integer::New(status) };
    argv[0] = Integer::New(status);
    MakeCallback(wrap->object_, "onclose", 1, argv);

    // Close the scope before we destruct this object. Probably nothing is
    // wrong if the scope is destroyed after, I just don't want the
    // possibility of a bug.
    scope.Close(v8::Undefined());

    delete wrap;
  }

  uv_handle_t handle_;
  Persistent<Object> object_;
};


}  // namespace node

NODE_MODULE(node_timer_wrap, node::TimerWrap::Initialize);
