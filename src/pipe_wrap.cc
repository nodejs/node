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

#include "pipe_wrap.h"

#include "async-wrap.h"
#include "env.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "node.h"
#include "node_buffer.h"
#include "node_wrap.h"
#include "req_wrap.h"
#include "stream_wrap.h"
#include "util-inl.h"
#include "util.h"

namespace node {

using v8::Boolean;
using v8::Context;
using v8::EscapableHandleScope;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::PropertyAttribute;
using v8::String;
using v8::Undefined;
using v8::Value;


// TODO(bnoordhuis) share with TCPWrap?
class PipeConnectWrap : public ReqWrap<uv_connect_t> {
 public:
  PipeConnectWrap(Environment* env, Local<Object> req_wrap_obj);
};


PipeConnectWrap::PipeConnectWrap(Environment* env, Local<Object> req_wrap_obj)
    : ReqWrap<uv_connect_t>(env, req_wrap_obj, AsyncWrap::PROVIDER_PIPEWRAP) {
  Wrap(req_wrap_obj, this);
}


static void NewPipeConnectWrap(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
}


uv_pipe_t* PipeWrap::UVHandle() {
  return &handle_;
}


Local<Object> PipeWrap::Instantiate(Environment* env, AsyncWrap* parent) {
  EscapableHandleScope handle_scope(env->isolate());
  assert(!env->pipe_constructor_template().IsEmpty());
  Local<Function> constructor = env->pipe_constructor_template()->GetFunction();
  assert(!constructor.IsEmpty());
  Local<Value> ptr = External::New(env->isolate(), parent);
  Local<Object> instance = constructor->NewInstance(1, &ptr);
  assert(!instance.IsEmpty());
  return handle_scope.Escape(instance);
}


void PipeWrap::Initialize(Handle<Object> target,
                          Handle<Value> unused,
                          Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Local<FunctionTemplate> t = FunctionTemplate::New(env->isolate(), New);
  t->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Pipe"));
  t->InstanceTemplate()->SetInternalFieldCount(1);

  enum PropertyAttribute attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  t->InstanceTemplate()->SetAccessor(env->fd_string(),
                                     StreamWrap::GetFD,
                                     NULL,
                                     Handle<Value>(),
                                     v8::DEFAULT,
                                     attributes);

  NODE_SET_PROTOTYPE_METHOD(t, "close", HandleWrap::Close);
  NODE_SET_PROTOTYPE_METHOD(t, "unref", HandleWrap::Unref);
  NODE_SET_PROTOTYPE_METHOD(t, "ref", HandleWrap::Ref);

  NODE_SET_PROTOTYPE_METHOD(t, "setBlocking", StreamWrap::SetBlocking);

  NODE_SET_PROTOTYPE_METHOD(t, "readStart", StreamWrap::ReadStart);
  NODE_SET_PROTOTYPE_METHOD(t, "readStop", StreamWrap::ReadStop);
  NODE_SET_PROTOTYPE_METHOD(t, "shutdown", StreamWrap::Shutdown);

  NODE_SET_PROTOTYPE_METHOD(t, "writeBuffer", StreamWrap::WriteBuffer);
  NODE_SET_PROTOTYPE_METHOD(t,
                            "writeAsciiString",
                            StreamWrap::WriteAsciiString);
  NODE_SET_PROTOTYPE_METHOD(t, "writeUtf8String", StreamWrap::WriteUtf8String);
  NODE_SET_PROTOTYPE_METHOD(t, "writeUcs2String", StreamWrap::WriteUcs2String);
  NODE_SET_PROTOTYPE_METHOD(t,
                            "writeBinaryString",
                            StreamWrap::WriteBinaryString);

  NODE_SET_PROTOTYPE_METHOD(t, "bind", Bind);
  NODE_SET_PROTOTYPE_METHOD(t, "listen", Listen);
  NODE_SET_PROTOTYPE_METHOD(t, "connect", Connect);
  NODE_SET_PROTOTYPE_METHOD(t, "open", Open);

#ifdef _WIN32
  NODE_SET_PROTOTYPE_METHOD(t, "setPendingInstances", SetPendingInstances);
#endif

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "Pipe"), t->GetFunction());
  env->set_pipe_constructor_template(t);

  // Create FunctionTemplate for PipeConnectWrap.
  Local<FunctionTemplate> cwt =
      FunctionTemplate::New(env->isolate(), NewPipeConnectWrap);
  cwt->InstanceTemplate()->SetInternalFieldCount(1);
  cwt->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "PipeConnectWrap"));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "PipeConnectWrap"),
              cwt->GetFunction());
}


void PipeWrap::New(const FunctionCallbackInfo<Value>& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  assert(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  if (args[0]->IsExternal()) {
    void* ptr = args[0].As<External>()->Value();
    new PipeWrap(env, args.This(), false, static_cast<AsyncWrap*>(ptr));
  } else {
    new PipeWrap(env, args.This(), args[0]->IsTrue(), NULL);
  }
}


PipeWrap::PipeWrap(Environment* env,
                   Handle<Object> object,
                   bool ipc,
                   AsyncWrap* parent)
    : StreamWrap(env,
                 object,
                 reinterpret_cast<uv_stream_t*>(&handle_),
                 AsyncWrap::PROVIDER_PIPEWRAP,
                 parent) {
  int r = uv_pipe_init(env->event_loop(), &handle_, ipc);
  assert(r == 0);  // How do we proxy this error up to javascript?
                   // Suggestion: uv_pipe_init() returns void.
  UpdateWriteQueueSize();
}


void PipeWrap::Bind(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  PipeWrap* wrap = Unwrap<PipeWrap>(args.Holder());

  node::Utf8Value name(args[0]);
  int err = uv_pipe_bind(&wrap->handle_, *name);
  args.GetReturnValue().Set(err);
}


#ifdef _WIN32
void PipeWrap::SetPendingInstances(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  PipeWrap* wrap = Unwrap<PipeWrap>(args.Holder());

  int instances = args[0]->Int32Value();

  uv_pipe_pending_instances(&wrap->handle_, instances);
}
#endif


void PipeWrap::Listen(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  PipeWrap* wrap = Unwrap<PipeWrap>(args.Holder());

  int backlog = args[0]->Int32Value();
  int err = uv_listen(reinterpret_cast<uv_stream_t*>(&wrap->handle_),
                      backlog,
                      OnConnection);
  args.GetReturnValue().Set(err);
}


// TODO(bnoordhuis) maybe share with TCPWrap?
void PipeWrap::OnConnection(uv_stream_t* handle, int status) {
  PipeWrap* pipe_wrap = static_cast<PipeWrap*>(handle->data);
  assert(&pipe_wrap->handle_ == reinterpret_cast<uv_pipe_t*>(handle));

  Environment* env = pipe_wrap->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  // We should not be getting this callback if someone as already called
  // uv_close() on the handle.
  assert(pipe_wrap->persistent().IsEmpty() == false);

  Local<Value> argv[] = {
    Integer::New(env->isolate(), status),
    Undefined(env->isolate())
  };

  if (status != 0) {
    pipe_wrap->MakeCallback(env->onconnection_string(), ARRAY_SIZE(argv), argv);
    return;
  }

  // Instanciate the client javascript object and handle.
  Local<Object> client_obj = Instantiate(env, pipe_wrap);

  // Unwrap the client javascript object.
  PipeWrap* wrap = Unwrap<PipeWrap>(client_obj);
  uv_stream_t* client_handle = reinterpret_cast<uv_stream_t*>(&wrap->handle_);
  if (uv_accept(handle, client_handle))
    return;

  // Successful accept. Call the onconnection callback in JavaScript land.
  argv[1] = client_obj;
  pipe_wrap->MakeCallback(env->onconnection_string(), ARRAY_SIZE(argv), argv);
}

// TODO(bnoordhuis) Maybe share this with TCPWrap?
void PipeWrap::AfterConnect(uv_connect_t* req, int status) {
  PipeConnectWrap* req_wrap = static_cast<PipeConnectWrap*>(req->data);
  PipeWrap* wrap = static_cast<PipeWrap*>(req->handle->data);
  assert(req_wrap->env() == wrap->env());
  Environment* env = wrap->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  // The wrap and request objects should still be there.
  assert(req_wrap->persistent().IsEmpty() == false);
  assert(wrap->persistent().IsEmpty() == false);

  bool readable, writable;

  if (status) {
    readable = writable = 0;
  } else {
    readable = uv_is_readable(req->handle) != 0;
    writable = uv_is_writable(req->handle) != 0;
  }

  Local<Object> req_wrap_obj = req_wrap->object();
  Local<Value> argv[5] = {
    Integer::New(env->isolate(), status),
    wrap->object(),
    req_wrap_obj,
    Boolean::New(env->isolate(), readable),
    Boolean::New(env->isolate(), writable)
  };

  req_wrap->MakeCallback(env->oncomplete_string(), ARRAY_SIZE(argv), argv);

  delete req_wrap;
}


void PipeWrap::Open(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  PipeWrap* wrap = Unwrap<PipeWrap>(args.Holder());

  int fd = args[0]->Int32Value();

  int err = uv_pipe_open(&wrap->handle_, fd);

  if (err != 0)
    env->isolate()->ThrowException(UVException(err, "uv_pipe_open"));
}


void PipeWrap::Connect(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());

  PipeWrap* wrap = Unwrap<PipeWrap>(args.Holder());

  assert(args[0]->IsObject());
  assert(args[1]->IsString());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  node::Utf8Value name(args[1]);

  PipeConnectWrap* req_wrap = new PipeConnectWrap(env, req_wrap_obj);
  uv_pipe_connect(&req_wrap->req_,
                  &wrap->handle_,
                  *name,
                  AfterConnect);
  req_wrap->Dispatched();

  args.GetReturnValue().Set(0);  // uv_pipe_connect() doesn't return errors.
}


}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(pipe_wrap, node::PipeWrap::Initialize)
