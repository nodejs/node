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

#include "tcp_wrap.h"

#include "env.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "node_buffer.h"
#include "node_wrap.h"
#include "req_wrap.h"
#include "stream_wrap.h"
#include "util.h"
#include "util-inl.h"

#include <stdlib.h>


namespace node {

using v8::Context;
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

typedef class ReqWrap<uv_connect_t> ConnectWrap;


Local<Object> TCPWrap::Instantiate(Environment* env) {
  HandleScope handle_scope(env->isolate());
  assert(env->tcp_constructor_template().IsEmpty() == false);
  Local<Function> constructor = env->tcp_constructor_template()->GetFunction();
  assert(constructor.IsEmpty() == false);
  Local<Object> instance = constructor->NewInstance();
  assert(instance.IsEmpty() == false);
  return handle_scope.Close(instance);
}


void TCPWrap::Initialize(Handle<Object> target,
                         Handle<Value> unused,
                         Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  t->SetClassName(FIXED_ONE_BYTE_STRING(node_isolate, "TCP"));
  t->InstanceTemplate()->SetInternalFieldCount(1);

  enum PropertyAttribute attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  t->InstanceTemplate()->SetAccessor(FIXED_ONE_BYTE_STRING(node_isolate, "fd"),
                                     StreamWrap::GetFD,
                                     NULL,
                                     Handle<Value>(),
                                     v8::DEFAULT,
                                     attributes);

  NODE_SET_PROTOTYPE_METHOD(t, "close", HandleWrap::Close);

  NODE_SET_PROTOTYPE_METHOD(t, "ref", HandleWrap::Ref);
  NODE_SET_PROTOTYPE_METHOD(t, "unref", HandleWrap::Unref);

  NODE_SET_PROTOTYPE_METHOD(t, "readStart", StreamWrap::ReadStart);
  NODE_SET_PROTOTYPE_METHOD(t, "readStop", StreamWrap::ReadStop);
  NODE_SET_PROTOTYPE_METHOD(t, "shutdown", StreamWrap::Shutdown);

  NODE_SET_PROTOTYPE_METHOD(t, "writeBuffer", StreamWrap::WriteBuffer);
  NODE_SET_PROTOTYPE_METHOD(t,
                            "writeAsciiString",
                            StreamWrap::WriteAsciiString);
  NODE_SET_PROTOTYPE_METHOD(t, "writeUtf8String", StreamWrap::WriteUtf8String);
  NODE_SET_PROTOTYPE_METHOD(t, "writeUcs2String", StreamWrap::WriteUcs2String);
  NODE_SET_PROTOTYPE_METHOD(t, "writev", StreamWrap::Writev);

  NODE_SET_PROTOTYPE_METHOD(t, "open", Open);
  NODE_SET_PROTOTYPE_METHOD(t, "bind", Bind);
  NODE_SET_PROTOTYPE_METHOD(t, "listen", Listen);
  NODE_SET_PROTOTYPE_METHOD(t, "connect", Connect);
  NODE_SET_PROTOTYPE_METHOD(t, "bind6", Bind6);
  NODE_SET_PROTOTYPE_METHOD(t, "connect6", Connect6);
  NODE_SET_PROTOTYPE_METHOD(t, "getsockname", GetSockName);
  NODE_SET_PROTOTYPE_METHOD(t, "getpeername", GetPeerName);
  NODE_SET_PROTOTYPE_METHOD(t, "setNoDelay", SetNoDelay);
  NODE_SET_PROTOTYPE_METHOD(t, "setKeepAlive", SetKeepAlive);

#ifdef _WIN32
  NODE_SET_PROTOTYPE_METHOD(t,
                            "setSimultaneousAccepts",
                            SetSimultaneousAccepts);
#endif

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "TCP"), t->GetFunction());
  env->set_tcp_constructor_template(t);
}


uv_tcp_t* TCPWrap::UVHandle() {
  return &handle_;
}


void TCPWrap::New(const FunctionCallbackInfo<Value>& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  assert(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());
  TCPWrap* wrap = new TCPWrap(env, args.This());
  assert(wrap);
}


TCPWrap::TCPWrap(Environment* env, Handle<Object> object)
    : StreamWrap(env, object, reinterpret_cast<uv_stream_t*>(&handle_)) {
  int r = uv_tcp_init(env->event_loop(), &handle_);
  assert(r == 0);  // How do we proxy this error up to javascript?
                   // Suggestion: uv_tcp_init() returns void.
  UpdateWriteQueueSize();
}


TCPWrap::~TCPWrap() {
  assert(persistent().IsEmpty());
}


void TCPWrap::GetSockName(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());
  struct sockaddr_storage address;

  TCPWrap* wrap = Unwrap<TCPWrap>(args.This());

  assert(args[0]->IsObject());
  Local<Object> out = args[0].As<Object>();

  int addrlen = sizeof(address);
  int err = uv_tcp_getsockname(&wrap->handle_,
                               reinterpret_cast<sockaddr*>(&address),
                               &addrlen);
  if (err == 0) {
    const sockaddr* addr = reinterpret_cast<const sockaddr*>(&address);
    AddressToJS(env, addr, out);
  }

  args.GetReturnValue().Set(err);
}


void TCPWrap::GetPeerName(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());
  struct sockaddr_storage address;

  TCPWrap* wrap = Unwrap<TCPWrap>(args.This());

  assert(args[0]->IsObject());
  Local<Object> out = args[0].As<Object>();

  int addrlen = sizeof(address);
  int err = uv_tcp_getpeername(&wrap->handle_,
                               reinterpret_cast<sockaddr*>(&address),
                               &addrlen);
  if (err == 0) {
    const sockaddr* addr = reinterpret_cast<const sockaddr*>(&address);
    AddressToJS(env, addr, out);
  }

  args.GetReturnValue().Set(err);
}


void TCPWrap::SetNoDelay(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap = Unwrap<TCPWrap>(args.This());

  int enable = static_cast<int>(args[0]->BooleanValue());
  int err = uv_tcp_nodelay(&wrap->handle_, enable);
  args.GetReturnValue().Set(err);
}


void TCPWrap::SetKeepAlive(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap = Unwrap<TCPWrap>(args.This());

  int enable = args[0]->Int32Value();
  unsigned int delay = args[1]->Uint32Value();

  int err = uv_tcp_keepalive(&wrap->handle_, enable, delay);
  args.GetReturnValue().Set(err);
}


#ifdef _WIN32
void TCPWrap::SetSimultaneousAccepts(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap = Unwrap<TCPWrap>(args.This());

  bool enable = args[0]->BooleanValue();
  int err = uv_tcp_simultaneous_accepts(&wrap->handle_, enable);
  args.GetReturnValue().Set(err);
}
#endif


void TCPWrap::Open(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  TCPWrap* wrap = Unwrap<TCPWrap>(args.This());
  int fd = args[0]->IntegerValue();
  uv_tcp_open(&wrap->handle_, fd);
}


void TCPWrap::Bind(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap = Unwrap<TCPWrap>(args.This());

  String::AsciiValue ip_address(args[0]);
  int port = args[1]->Int32Value();

  sockaddr_in addr;
  int err = uv_ip4_addr(*ip_address, port, &addr);
  if (err == 0)
    err = uv_tcp_bind(&wrap->handle_, reinterpret_cast<const sockaddr*>(&addr));

  args.GetReturnValue().Set(err);
}


void TCPWrap::Bind6(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap = Unwrap<TCPWrap>(args.This());

  String::AsciiValue ip6_address(args[0]);
  int port = args[1]->Int32Value();

  sockaddr_in6 addr;
  int err = uv_ip6_addr(*ip6_address, port, &addr);
  if (err == 0)
    err = uv_tcp_bind(&wrap->handle_, reinterpret_cast<const sockaddr*>(&addr));

  args.GetReturnValue().Set(err);
}


void TCPWrap::Listen(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap = Unwrap<TCPWrap>(args.This());

  int backlog = args[0]->Int32Value();
  int err = uv_listen(reinterpret_cast<uv_stream_t*>(&wrap->handle_),
                      backlog,
                      OnConnection);
  args.GetReturnValue().Set(err);
}


void TCPWrap::OnConnection(uv_stream_t* handle, int status) {
  TCPWrap* tcp_wrap = static_cast<TCPWrap*>(handle->data);
  assert(&tcp_wrap->handle_ == reinterpret_cast<uv_tcp_t*>(handle));
  Environment* env = tcp_wrap->env();

  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());

  // We should not be getting this callback if someone as already called
  // uv_close() on the handle.
  assert(tcp_wrap->persistent().IsEmpty() == false);

  Local<Value> argv[2] = {
    Integer::New(status, node_isolate),
    Undefined()
  };

  if (status == 0) {
    // Instantiate the client javascript object and handle.
    Local<Object> client_obj = Instantiate(env);

    // Unwrap the client javascript object.
    TCPWrap* wrap = Unwrap<TCPWrap>(client_obj);
    uv_stream_t* client_handle = reinterpret_cast<uv_stream_t*>(&wrap->handle_);
    if (uv_accept(handle, client_handle))
      return;

    // Successful accept. Call the onconnection callback in JavaScript land.
    argv[1] = client_obj;
  }

  MakeCallback(env,
               tcp_wrap->object(),
               env->onconnection_string(),
               ARRAY_SIZE(argv),
               argv);
}


void TCPWrap::AfterConnect(uv_connect_t* req, int status) {
  ConnectWrap* req_wrap = static_cast<ConnectWrap*>(req->data);
  TCPWrap* wrap = static_cast<TCPWrap*>(req->handle->data);
  assert(req_wrap->env() == wrap->env());
  Environment* env = wrap->env();

  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());

  // The wrap and request objects should still be there.
  assert(req_wrap->persistent().IsEmpty() == false);
  assert(wrap->persistent().IsEmpty() == false);

  Local<Object> req_wrap_obj = req_wrap->object();
  Local<Value> argv[5] = {
    Integer::New(status, node_isolate),
    wrap->object(),
    req_wrap_obj,
    v8::True(node_isolate),
    v8::True(node_isolate)
  };
  MakeCallback(env,
               req_wrap_obj,
               env->oncomplete_string(),
               ARRAY_SIZE(argv),
               argv);

  delete req_wrap;
}


void TCPWrap::Connect(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  TCPWrap* wrap = Unwrap<TCPWrap>(args.This());

  assert(args[0]->IsObject());
  assert(args[1]->IsString());
  assert(args[2]->Uint32Value());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  String::AsciiValue ip_address(args[1]);
  int port = args[2]->Uint32Value();

  sockaddr_in addr;
  int err = uv_ip4_addr(*ip_address, port, &addr);

  if (err == 0) {
    ConnectWrap* req_wrap = new ConnectWrap(env, req_wrap_obj);
    err = uv_tcp_connect(&req_wrap->req_,
                         &wrap->handle_,
                         reinterpret_cast<const sockaddr*>(&addr),
                         AfterConnect);
    req_wrap->Dispatched();
    if (err)
      delete req_wrap;
  }

  args.GetReturnValue().Set(err);
}


void TCPWrap::Connect6(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  TCPWrap* wrap = Unwrap<TCPWrap>(args.This());

  assert(args[0]->IsObject());
  assert(args[1]->IsString());
  assert(args[2]->Uint32Value());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  String::AsciiValue ip_address(args[1]);
  int port = args[2]->Int32Value();

  sockaddr_in6 addr;
  int err = uv_ip6_addr(*ip_address, port, &addr);

  if (err == 0) {
    ConnectWrap* req_wrap = new ConnectWrap(env, req_wrap_obj);
    err = uv_tcp_connect(&req_wrap->req_,
                         &wrap->handle_,
                         reinterpret_cast<const sockaddr*>(&addr),
                         AfterConnect);
    req_wrap->Dispatched();
    if (err)
      delete req_wrap;
  }

  args.GetReturnValue().Set(err);
}


// also used by udp_wrap.cc
Local<Object> AddressToJS(Environment* env,
                          const sockaddr* addr,
                          Local<Object> info) {
  HandleScope scope(node_isolate);
  char ip[INET6_ADDRSTRLEN];
  const sockaddr_in *a4;
  const sockaddr_in6 *a6;
  int port;

  if (info.IsEmpty())
    info = Object::New();

  switch (addr->sa_family) {
  case AF_INET6:
    a6 = reinterpret_cast<const sockaddr_in6*>(addr);
    uv_inet_ntop(AF_INET6, &a6->sin6_addr, ip, sizeof ip);
    port = ntohs(a6->sin6_port);
    info->Set(env->address_string(), OneByteString(node_isolate, ip));
    info->Set(env->family_string(), env->ipv6_string());
    info->Set(env->port_string(), Integer::New(port, node_isolate));
    break;

  case AF_INET:
    a4 = reinterpret_cast<const sockaddr_in*>(addr);
    uv_inet_ntop(AF_INET, &a4->sin_addr, ip, sizeof ip);
    port = ntohs(a4->sin_port);
    info->Set(env->address_string(), OneByteString(node_isolate, ip));
    info->Set(env->family_string(), env->ipv4_string());
    info->Set(env->port_string(), Integer::New(port, node_isolate));
    break;

  default:
    info->Set(env->address_string(), String::Empty(node_isolate));
  }

  return scope.Close(info);
}


}  // namespace node

NODE_MODULE_CONTEXT_AWARE(node_tcp_wrap, node::TCPWrap::Initialize)
