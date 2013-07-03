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
#include "node_buffer.h"
#include "req_wrap.h"
#include "handle_wrap.h"
#include "stream_wrap.h"
#include "pipe_wrap.h"
#include "node_wrap.h"

namespace node {

using v8::Boolean;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::Persistent;
using v8::PropertyAttribute;
using v8::String;
using v8::Value;

static Persistent<Function> pipeConstructor;
static Cached<String> onconnection_sym;
static Cached<String> oncomplete_sym;


// TODO share with TCPWrap?
typedef class ReqWrap<uv_connect_t> ConnectWrap;


uv_pipe_t* PipeWrap::UVHandle() {
  return &handle_;
}


Local<Object> PipeWrap::Instantiate() {
  HandleScope scope(node_isolate);
  assert(!pipeConstructor.IsEmpty());
  return scope.Close(NewInstance(pipeConstructor));
}


PipeWrap* PipeWrap::Unwrap(Local<Object> obj) {
  assert(!obj.IsEmpty());
  assert(obj->InternalFieldCount() > 0);
  return static_cast<PipeWrap*>(obj->GetAlignedPointerFromInternalField(0));
}


void PipeWrap::Initialize(Handle<Object> target) {
  StreamWrap::Initialize(target);

  HandleScope scope(node_isolate);

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  t->SetClassName(String::NewSymbol("Pipe"));

  t->InstanceTemplate()->SetInternalFieldCount(1);

  enum PropertyAttribute attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  t->InstanceTemplate()->SetAccessor(String::New("fd"),
                                     StreamWrap::GetFD,
                                     NULL,
                                     Handle<Value>(),
                                     v8::DEFAULT,
                                     attributes);

  NODE_SET_PROTOTYPE_METHOD(t, "close", HandleWrap::Close);
  NODE_SET_PROTOTYPE_METHOD(t, "unref", HandleWrap::Unref);
  NODE_SET_PROTOTYPE_METHOD(t, "ref", HandleWrap::Ref);

  NODE_SET_PROTOTYPE_METHOD(t, "readStart", StreamWrap::ReadStart);
  NODE_SET_PROTOTYPE_METHOD(t, "readStop", StreamWrap::ReadStop);
  NODE_SET_PROTOTYPE_METHOD(t, "shutdown", StreamWrap::Shutdown);

  NODE_SET_PROTOTYPE_METHOD(t, "writeBuffer", StreamWrap::WriteBuffer);
  NODE_SET_PROTOTYPE_METHOD(t, "writeAsciiString", StreamWrap::WriteAsciiString);
  NODE_SET_PROTOTYPE_METHOD(t, "writeUtf8String", StreamWrap::WriteUtf8String);
  NODE_SET_PROTOTYPE_METHOD(t, "writeUcs2String", StreamWrap::WriteUcs2String);

  NODE_SET_PROTOTYPE_METHOD(t, "bind", Bind);
  NODE_SET_PROTOTYPE_METHOD(t, "listen", Listen);
  NODE_SET_PROTOTYPE_METHOD(t, "connect", Connect);
  NODE_SET_PROTOTYPE_METHOD(t, "open", Open);

#ifdef _WIN32
  NODE_SET_PROTOTYPE_METHOD(t, "setPendingInstances", SetPendingInstances);
#endif

  pipeConstructorTmpl.Reset(node_isolate, t);
  pipeConstructor.Reset(node_isolate, t->GetFunction());
  target->Set(String::NewSymbol("Pipe"), t->GetFunction());
}


void PipeWrap::New(const FunctionCallbackInfo<Value>& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  assert(args.IsConstructCall());

  HandleScope scope(node_isolate);
  PipeWrap* wrap = new PipeWrap(args.This(), args[0]->IsTrue());
  assert(wrap);
}


PipeWrap::PipeWrap(Handle<Object> object, bool ipc)
    : StreamWrap(object, reinterpret_cast<uv_stream_t*>(&handle_)) {
  int r = uv_pipe_init(uv_default_loop(), &handle_, ipc);
  assert(r == 0); // How do we proxy this error up to javascript?
                  // Suggestion: uv_pipe_init() returns void.
  UpdateWriteQueueSize();
}


void PipeWrap::Bind(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  UNWRAP(PipeWrap)

  String::AsciiValue name(args[0]);

  int r = uv_pipe_bind(&wrap->handle_, *name);

  // Error starting the pipe.
  if (r) SetErrno(uv_last_error(uv_default_loop()));

  args.GetReturnValue().Set(r);
}


#ifdef _WIN32
void PipeWrap::SetPendingInstances(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  UNWRAP(PipeWrap)

  int instances = args[0]->Int32Value();

  uv_pipe_pending_instances(&wrap->handle_, instances);
}
#endif


void PipeWrap::Listen(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  UNWRAP(PipeWrap)

  int backlog = args[0]->Int32Value();

  int r = uv_listen(reinterpret_cast<uv_stream_t*>(&wrap->handle_),
                    backlog,
                    OnConnection);

  // Error starting the pipe.
  if (r) SetErrno(uv_last_error(uv_default_loop()));

  args.GetReturnValue().Set(r);
}


// TODO maybe share with TCPWrap?
void PipeWrap::OnConnection(uv_stream_t* handle, int status) {
  HandleScope scope(node_isolate);

  PipeWrap* wrap = static_cast<PipeWrap*>(handle->data);
  assert(&wrap->handle_ == reinterpret_cast<uv_pipe_t*>(handle));

  // We should not be getting this callback if someone as already called
  // uv_close() on the handle.
  assert(wrap->persistent().IsEmpty() == false);

  if (status != 0) {
    SetErrno(uv_last_error(uv_default_loop()));
    MakeCallback(wrap->object(), "onconnection", 0, NULL);
    return;
  }

  // Instanciate the client javascript object and handle.
  Local<Object> client_obj = NewInstance(pipeConstructor);

  // Unwrap the client javascript object.
  assert(client_obj->InternalFieldCount() > 0);
  PipeWrap* client_wrap =
      static_cast<PipeWrap*>(client_obj->GetAlignedPointerFromInternalField(0));
  uv_stream_t* client_handle =
      reinterpret_cast<uv_stream_t*>(&client_wrap->handle_);
  if (uv_accept(handle, client_handle)) return;

  // Successful accept. Call the onconnection callback in JavaScript land.
  Local<Value> argv[1] = { client_obj };
  if (onconnection_sym.IsEmpty()) {
    onconnection_sym = String::New("onconnection");
  }
  MakeCallback(wrap->object(), onconnection_sym, ARRAY_SIZE(argv), argv);
}

// TODO Maybe share this with TCPWrap?
void PipeWrap::AfterConnect(uv_connect_t* req, int status) {
  ConnectWrap* req_wrap = (ConnectWrap*) req->data;
  PipeWrap* wrap = (PipeWrap*) req->handle->data;

  HandleScope scope(node_isolate);

  // The wrap and request objects should still be there.
  assert(req_wrap->persistent().IsEmpty() == false);
  assert(wrap->persistent().IsEmpty() == false);

  bool readable, writable;

  if (status) {
    SetErrno(uv_last_error(uv_default_loop()));
    readable = writable = 0;
  } else {
    readable = uv_is_readable(req->handle) != 0;
    writable = uv_is_writable(req->handle) != 0;
  }

  Local<Object> req_wrap_obj = req_wrap->object();
  Local<Value> argv[5] = {
    Integer::New(status, node_isolate),
    wrap->object(),
    req_wrap_obj,
    Local<Value>::New(node_isolate, Boolean::New(readable)),
    Local<Value>::New(node_isolate, Boolean::New(writable))
  };

  if (oncomplete_sym.IsEmpty()) {
    oncomplete_sym = String::New("oncomplete");
  }
  MakeCallback(req_wrap_obj, oncomplete_sym, ARRAY_SIZE(argv), argv);

  delete req_wrap;
}


void PipeWrap::Open(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  UNWRAP(PipeWrap)

  int fd = args[0]->IntegerValue();

  uv_pipe_open(&wrap->handle_, fd);
}


void PipeWrap::Connect(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  UNWRAP(PipeWrap)

  String::AsciiValue name(args[0]);

  ConnectWrap* req_wrap = new ConnectWrap();

  uv_pipe_connect(&req_wrap->req_,
                  &wrap->handle_,
                  *name,
                  AfterConnect);

  req_wrap->Dispatched();

  args.GetReturnValue().Set(req_wrap->persistent());
}


}  // namespace node

NODE_MODULE(node_pipe_wrap, node::PipeWrap::Initialize)
