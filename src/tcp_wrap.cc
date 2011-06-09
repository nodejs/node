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
//
// - The uv_close_cb is used to free the c++ object. The close callback
//   is not made into javascript land.
//
// - uv_ref, uv_unref counts are managed at this layer to avoid needless
//   js/c++ boundary crossing. At the javascript layer that should all be
//   taken care of.


#define UNWRAP \
  assert(!args.Holder().IsEmpty()); \
  assert(args.Holder()->InternalFieldCount() > 0); \
  TCPWrap* wrap =  \
      static_cast<TCPWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
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


class TCPWrap {
 public:
  static void Initialize(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> constructor = FunctionTemplate::New(New);
    constructor->InstanceTemplate()->SetInternalFieldCount(1);
    constructor->SetClassName(String::NewSymbol("TCP"));

    NODE_SET_PROTOTYPE_METHOD(constructor, "bind", Bind);
    NODE_SET_PROTOTYPE_METHOD(constructor, "close", Close);

    target->Set(String::NewSymbol("TCP"), constructor->GetFunction());
  }

 private:
  static Handle<Value> New(const Arguments& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());

    HandleScope scope;
    TCPWrap *wrap = new TCPWrap(args.This());
    assert(wrap);

    return scope.Close(args.This());
  }

  TCPWrap(Handle<Object> object) {
    int r = uv_tcp_init(&handle_);
    handle_.data = this;
    assert(r == 0); // How do we proxy this error up to javascript?
                    // Suggestion: uv_tcp_init() returns void.
    assert(object_.IsEmpty());
    assert(object->InternalFieldCount() > 0);
    object_ = v8::Persistent<v8::Object>::New(object);
    object_->SetPointerInInternalField(0, this);
  }

  ~TCPWrap() {
    assert(!object_.IsEmpty());
    object_->SetPointerInInternalField(0, NULL);
    object_.Dispose();
  }

  // Free the C++ object on the close callback.
  static void OnClose(uv_handle_t* handle) {
    TCPWrap* wrap = static_cast<TCPWrap*>(handle->data);
    delete wrap;
  }

  static Handle<Value> Bind(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    String::AsciiValue ip_address(args[0]->ToString());
    int port = args[1]->Int32Value();

    struct sockaddr_in address = uv_ip4_addr(*ip_address, port);
    int r = uv_bind(&wrap->handle_, address);

    // Error starting the tcp.
    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  // TODO: share me?
  static Handle<Value> Close(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_close((uv_handle_t*) &wrap->handle_, OnClose);

    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  uv_tcp_t handle_;
  Persistent<Object> object_;
};


}  // namespace node

NODE_MODULE(node_tcp_wrap, node::TCPWrap::Initialize);
