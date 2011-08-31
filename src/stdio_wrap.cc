#include <node.h>
#include <node_buffer.h>
#include <req_wrap.h>
#include <handle_wrap.h>
#include <stream_wrap.h>

#define UNWRAP \
  assert(!args.Holder().IsEmpty()); \
  assert(args.Holder()->InternalFieldCount() > 0); \
  StdIOWrap* wrap =  \
      static_cast<StdIOWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
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
using v8::Undefined;

extern Persistent<Function> tcpConstructor;
extern Persistent<Function> pipeConstructor;
static Persistent<Function> constructor;


class StdIOWrap : StreamWrap {
 public:
  static void Initialize(Handle<Object> target) {
    StreamWrap::Initialize(target);

    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->SetClassName(String::NewSymbol("StdIO"));

    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "readStart", StreamWrap::ReadStart);
    NODE_SET_PROTOTYPE_METHOD(t, "readStop", StreamWrap::ReadStop);
    NODE_SET_PROTOTYPE_METHOD(t, "write", StreamWrap::Write);
    NODE_SET_PROTOTYPE_METHOD(t, "listen", Listen);

    constructor = Persistent<Function>::New(t->GetFunction());

    target->Set(String::NewSymbol("StdIO"), constructor);
  }

 private:
  static Handle<Value> New(const Arguments& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());

    uv_std_type stdHandleType = (uv_std_type)args[0]->Int32Value();

    assert(stdHandleType == UV_STDIN || stdHandleType == UV_STDOUT || stdHandleType == UV_STDERR);

    uv_stream_t* stdHandle = uv_std_handle(uv_default_loop(), stdHandleType);
    if (stdHandle) {
      HandleScope scope;
      StdIOWrap* wrap = new StdIOWrap(args.This());
      assert(wrap);

      wrap->handle_ = stdHandle;
      wrap->SetHandle((uv_handle_t*)stdHandle);
      wrap->UpdateWriteQueueSize();

      return scope.Close(args.This());
    } else {
      return Undefined();
    }
  }

  StdIOWrap(Handle<Object> object) : StreamWrap(object, NULL) {
  }

  static Handle<Value> Listen(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int backlog = args[0]->Int32Value();

    int r = uv_listen(wrap->handle_, SOMAXCONN, OnConnection);

    // Error starting the pipe.
    if (r) SetErrno(uv_last_error(uv_default_loop()).code);

    return scope.Close(Integer::New(r));
  }

  // TODO maybe share with TCPWrap?
  static void OnConnection(uv_stream_t* handle, int status) {
    HandleScope scope;
    Local<Object> client_obj;

    StdIOWrap* wrap = static_cast<StdIOWrap*>(handle->data);
    assert(wrap->handle_ == handle);

    // We should not be getting this callback if someone as already called
    // uv_close() on the handle.
    assert(wrap->object_.IsEmpty() == false);

    if (status != 0) {
      // TODO Handle server error (set errno and call onconnection with NULL)
      assert(0);
      return;
    }

    // Instanciate the client javascript object and handle.
    switch (handle->type) {
      case UV_TCP:
        client_obj = tcpConstructor->NewInstance();
        break;
      case UV_NAMED_PIPE:
        client_obj = pipeConstructor->NewInstance();
        break;
      default:
        assert(0);
        return;
    }

    // Unwrap the client javascript object.
    assert(client_obj->InternalFieldCount() > 0);
    StreamWrap* client_wrap =
        static_cast<StreamWrap*>(client_obj->GetPointerFromInternalField(0));

    int r = uv_accept(handle, client_wrap->GetStream());

    // uv_accept should always work.
    assert(r == 0);

    // Successful accept. Call the onconnection callback in JavaScript land.
    Local<Value> argv[1] = { client_obj };
    MakeCallback(wrap->object_, "onconnection", 1, argv);
  }

  uv_stream_t* handle_;
};

}  // namespace node

NODE_MODULE(node_stdio_wrap, node::StdIOWrap::Initialize);
