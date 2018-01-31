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

#include "connection_wrap.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "node_buffer.h"
#include "node_wrap.h"
#include "connect_wrap.h"
#include "stream_wrap.h"
#include "util-inl.h"

#include <stdlib.h>


namespace node {

using v8::Boolean;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

using AsyncHooks = Environment::AsyncHooks;


Local<Object> TCPWrap::Instantiate(Environment* env,
                                   AsyncWrap* parent,
                                   TCPWrap::SocketType type) {
  EscapableHandleScope handle_scope(env->isolate());
  AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(
    env, parent->get_async_id());
  CHECK_EQ(env->tcp_constructor_template().IsEmpty(), false);
  Local<Function> constructor = env->tcp_constructor_template()->GetFunction();
  CHECK_EQ(constructor.IsEmpty(), false);
  Local<Value> type_value = Int32::New(env->isolate(), type);
  Local<Object> instance =
      constructor->NewInstance(env->context(), 1, &type_value).ToLocalChecked();
  return handle_scope.Escape(instance);
}


void TCPWrap::Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);
  Local<String> tcpString = FIXED_ONE_BYTE_STRING(env->isolate(), "TCP");
  t->SetClassName(tcpString);
  t->InstanceTemplate()->SetInternalFieldCount(1);

  // Init properties
  t->InstanceTemplate()->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "reading"),
                             Boolean::New(env->isolate(), false));
  t->InstanceTemplate()->Set(env->owner_string(), Null(env->isolate()));
  t->InstanceTemplate()->Set(env->onread_string(), Null(env->isolate()));
  t->InstanceTemplate()->Set(env->onconnection_string(), Null(env->isolate()));

  AsyncWrap::AddWrapMethods(env, t, AsyncWrap::kFlagHasReset);

  env->SetProtoMethod(t, "close", HandleWrap::Close);

  env->SetProtoMethod(t, "ref", HandleWrap::Ref);
  env->SetProtoMethod(t, "unref", HandleWrap::Unref);
  env->SetProtoMethod(t, "hasRef", HandleWrap::HasRef);

  LibuvStreamWrap::AddMethods(env, t, StreamBase::kFlagHasWritev);

  env->SetProtoMethod(t, "open", Open);
  env->SetProtoMethod(t, "bind", Bind);
  env->SetProtoMethod(t, "listen", Listen);
  env->SetProtoMethod(t, "connect", Connect);
  env->SetProtoMethod(t, "bind6", Bind6);
  env->SetProtoMethod(t, "connect6", Connect6);
  env->SetProtoMethod(t, "getsockname",
                      GetSockOrPeerName<TCPWrap, uv_tcp_getsockname>);
  env->SetProtoMethod(t, "getpeername",
                      GetSockOrPeerName<TCPWrap, uv_tcp_getpeername>);
  env->SetProtoMethod(t, "setNoDelay", SetNoDelay);
  env->SetProtoMethod(t, "setKeepAlive", SetKeepAlive);

#ifdef _WIN32
  env->SetProtoMethod(t, "setSimultaneousAccepts", SetSimultaneousAccepts);
#endif

  target->Set(tcpString, t->GetFunction());
  env->set_tcp_constructor_template(t);

  // Create FunctionTemplate for TCPConnectWrap.
  auto constructor = [](const FunctionCallbackInfo<Value>& args) {
    CHECK(args.IsConstructCall());
    ClearWrap(args.This());
  };
  auto cwt = FunctionTemplate::New(env->isolate(), constructor);
  cwt->InstanceTemplate()->SetInternalFieldCount(1);
  AsyncWrap::AddWrapMethods(env, cwt);
  Local<String> wrapString =
      FIXED_ONE_BYTE_STRING(env->isolate(), "TCPConnectWrap");
  cwt->SetClassName(wrapString);
  target->Set(wrapString, cwt->GetFunction());

  // Define constants
  Local<Object> constants = Object::New(env->isolate());
  NODE_DEFINE_CONSTANT(constants, SOCKET);
  NODE_DEFINE_CONSTANT(constants, SERVER);
  target->Set(context,
              FIXED_ONE_BYTE_STRING(env->isolate(), "constants"),
              constants).FromJust();
}


void TCPWrap::New(const FunctionCallbackInfo<Value>& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsInt32());
  Environment* env = Environment::GetCurrent(args);

  int type_value = args[0].As<Int32>()->Value();
  TCPWrap::SocketType type = static_cast<TCPWrap::SocketType>(type_value);

  ProviderType provider;
  switch (type) {
    case SOCKET:
      provider = PROVIDER_TCPWRAP;
      break;
    case SERVER:
      provider = PROVIDER_TCPSERVERWRAP;
      break;
    default:
      UNREACHABLE();
  }

  new TCPWrap(env, args.This(), provider);
}


TCPWrap::TCPWrap(Environment* env, Local<Object> object, ProviderType provider)
    : ConnectionWrap(env, object, provider) {
  int r = uv_tcp_init(env->event_loop(), &handle_);
  CHECK_EQ(r, 0);  // How do we proxy this error up to javascript?
                   // Suggestion: uv_tcp_init() returns void.
}


TCPWrap::~TCPWrap() {
  CHECK(persistent().IsEmpty());
}


void TCPWrap::SetNoDelay(const FunctionCallbackInfo<Value>& args) {
  TCPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  int enable = static_cast<int>(args[0]->BooleanValue());
  int err = uv_tcp_nodelay(&wrap->handle_, enable);
  args.GetReturnValue().Set(err);
}


void TCPWrap::SetKeepAlive(const FunctionCallbackInfo<Value>& args) {
  TCPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  int enable = args[0]->Int32Value();
  unsigned int delay = args[1]->Uint32Value();
  int err = uv_tcp_keepalive(&wrap->handle_, enable, delay);
  args.GetReturnValue().Set(err);
}


#ifdef _WIN32
void TCPWrap::SetSimultaneousAccepts(const FunctionCallbackInfo<Value>& args) {
  TCPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  bool enable = args[0]->BooleanValue();
  int err = uv_tcp_simultaneous_accepts(&wrap->handle_, enable);
  args.GetReturnValue().Set(err);
}
#endif


void TCPWrap::Open(const FunctionCallbackInfo<Value>& args) {
  TCPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  int fd = static_cast<int>(args[0]->IntegerValue());
  uv_tcp_open(&wrap->handle_, fd);
}


void TCPWrap::Bind(const FunctionCallbackInfo<Value>& args) {
  TCPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  node::Utf8Value ip_address(args.GetIsolate(), args[0]);
  int port = args[1]->Int32Value();
  sockaddr_in addr;
  int err = uv_ip4_addr(*ip_address, port, &addr);
  if (err == 0) {
    err = uv_tcp_bind(&wrap->handle_,
                      reinterpret_cast<const sockaddr*>(&addr),
                      0);
  }
  args.GetReturnValue().Set(err);
}


void TCPWrap::Bind6(const FunctionCallbackInfo<Value>& args) {
  TCPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  node::Utf8Value ip6_address(args.GetIsolate(), args[0]);
  int port = args[1]->Int32Value();
  sockaddr_in6 addr;
  int err = uv_ip6_addr(*ip6_address, port, &addr);
  if (err == 0) {
    err = uv_tcp_bind(&wrap->handle_,
                      reinterpret_cast<const sockaddr*>(&addr),
                      0);
  }
  args.GetReturnValue().Set(err);
}


void TCPWrap::Listen(const FunctionCallbackInfo<Value>& args) {
  TCPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  int backlog = args[0]->Int32Value();
  int err = uv_listen(reinterpret_cast<uv_stream_t*>(&wrap->handle_),
                      backlog,
                      OnConnection);
  args.GetReturnValue().Set(err);
}


void TCPWrap::Connect(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  TCPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsUint32());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  node::Utf8Value ip_address(env->isolate(), args[1]);
  int port = args[2]->Uint32Value();

  sockaddr_in addr;
  int err = uv_ip4_addr(*ip_address, port, &addr);

  if (err == 0) {
    AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(
      env, wrap->get_async_id());
    ConnectWrap* req_wrap =
        new ConnectWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_TCPCONNECTWRAP);
    err = uv_tcp_connect(req_wrap->req(),
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
  Environment* env = Environment::GetCurrent(args);

  TCPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsUint32());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  node::Utf8Value ip_address(env->isolate(), args[1]);
  int port = args[2]->Int32Value();

  sockaddr_in6 addr;
  int err = uv_ip6_addr(*ip_address, port, &addr);

  if (err == 0) {
    AsyncHooks::DefaultTriggerAsyncIdScope trigger_scope(
      env, wrap->get_async_id());
    ConnectWrap* req_wrap =
        new ConnectWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_TCPCONNECTWRAP);
    err = uv_tcp_connect(req_wrap->req(),
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
  EscapableHandleScope scope(env->isolate());
  char ip[INET6_ADDRSTRLEN];
  const sockaddr_in *a4;
  const sockaddr_in6 *a6;
  int port;

  if (info.IsEmpty())
    info = Object::New(env->isolate());

  switch (addr->sa_family) {
  case AF_INET6:
    a6 = reinterpret_cast<const sockaddr_in6*>(addr);
    uv_inet_ntop(AF_INET6, &a6->sin6_addr, ip, sizeof ip);
    port = ntohs(a6->sin6_port);
    info->Set(env->address_string(), OneByteString(env->isolate(), ip));
    info->Set(env->family_string(), env->ipv6_string());
    info->Set(env->port_string(), Integer::New(env->isolate(), port));
    break;

  case AF_INET:
    a4 = reinterpret_cast<const sockaddr_in*>(addr);
    uv_inet_ntop(AF_INET, &a4->sin_addr, ip, sizeof ip);
    port = ntohs(a4->sin_port);
    info->Set(env->address_string(), OneByteString(env->isolate(), ip));
    info->Set(env->family_string(), env->ipv4_string());
    info->Set(env->port_string(), Integer::New(env->isolate(), port));
    break;

  default:
    info->Set(env->address_string(), String::Empty(env->isolate()));
  }

  return scope.Escape(info);
}


}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(tcp_wrap, node::TCPWrap::Initialize)
