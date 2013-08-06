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
#include "node.h"
#include "node_buffer.h"
#include "node_wrap.h"
#include "handle_wrap.h"
#include "req_wrap.h"
#include "stream_wrap.h"

#include <stdlib.h>


namespace node {

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
using v8::Undefined;
using v8::Value;

static Persistent<Function> tcpConstructor;
static Cached<String> oncomplete_sym;
static Cached<String> onconnection_sym;


typedef class ReqWrap<uv_connect_t> ConnectWrap;


Local<Object> TCPWrap::Instantiate() {
  // If this assert fire then process.binding('tcp_wrap') hasn't been
  // called yet.
  assert(tcpConstructor.IsEmpty() == false);

  HandleScope scope(node_isolate);
  Local<Object> obj = NewInstance(tcpConstructor);

  return scope.Close(obj);
}


void TCPWrap::Initialize(Handle<Object> target) {
  StreamWrap::Initialize(target);

  HandleScope scope(node_isolate);

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

  onconnection_sym = FIXED_ONE_BYTE_STRING(node_isolate, "onconnection");
  oncomplete_sym = FIXED_ONE_BYTE_STRING(node_isolate, "oncomplete");

  tcpConstructorTmpl.Reset(node_isolate, t);
  tcpConstructor.Reset(node_isolate, t->GetFunction());
  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "TCP"), t->GetFunction());
}


TCPWrap* TCPWrap::Unwrap(Local<Object> obj) {
  TCPWrap* wrap;
  UNWRAP(obj, TCPWrap, wrap);
  return wrap;
}


uv_tcp_t* TCPWrap::UVHandle() {
  return &handle_;
}


void TCPWrap::New(const FunctionCallbackInfo<Value>& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  assert(args.IsConstructCall());
  HandleScope scope(node_isolate);
  TCPWrap* wrap = new TCPWrap(args.This());
  assert(wrap);
}


TCPWrap::TCPWrap(Handle<Object> object)
    : StreamWrap(object, reinterpret_cast<uv_stream_t*>(&handle_)) {
  int r = uv_tcp_init(uv_default_loop(), &handle_);
  assert(r == 0);  // How do we proxy this error up to javascript?
                   // Suggestion: uv_tcp_init() returns void.
  UpdateWriteQueueSize();
}


TCPWrap::~TCPWrap() {
  assert(persistent().IsEmpty());
}


void TCPWrap::GetSockName(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  struct sockaddr_storage address;

  TCPWrap* wrap;
  UNWRAP(args.This(), TCPWrap, wrap);

  assert(args[0]->IsObject());
  Local<Object> out = args[0].As<Object>();

  int addrlen = sizeof(address);
  int err = uv_tcp_getsockname(&wrap->handle_,
                               reinterpret_cast<sockaddr*>(&address),
                               &addrlen);
  if (err == 0) {
    const sockaddr* addr = reinterpret_cast<const sockaddr*>(&address);
    AddressToJS(addr, out);
  }

  args.GetReturnValue().Set(err);
}


void TCPWrap::GetPeerName(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  struct sockaddr_storage address;

  TCPWrap* wrap;
  UNWRAP(args.This(), TCPWrap, wrap);

  assert(args[0]->IsObject());
  Local<Object> out = args[0].As<Object>();

  int addrlen = sizeof(address);
  int err = uv_tcp_getpeername(&wrap->handle_,
                               reinterpret_cast<sockaddr*>(&address),
                               &addrlen);
  if (err == 0) {
    const sockaddr* addr = reinterpret_cast<const sockaddr*>(&address);
    AddressToJS(addr, out);
  }

  args.GetReturnValue().Set(err);
}


void TCPWrap::SetNoDelay(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap;
  UNWRAP(args.This(), TCPWrap, wrap);

  int enable = static_cast<int>(args[0]->BooleanValue());
  int err = uv_tcp_nodelay(&wrap->handle_, enable);
  args.GetReturnValue().Set(err);
}


void TCPWrap::SetKeepAlive(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap;
  UNWRAP(args.This(), TCPWrap, wrap);

  int enable = args[0]->Int32Value();
  unsigned int delay = args[1]->Uint32Value();

  int err = uv_tcp_keepalive(&wrap->handle_, enable, delay);
  args.GetReturnValue().Set(err);
}


#ifdef _WIN32
void TCPWrap::SetSimultaneousAccepts(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap;
  UNWRAP(args.This(), TCPWrap, wrap);

  bool enable = args[0]->BooleanValue();
  int err = uv_tcp_simultaneous_accepts(&wrap->handle_, enable);
  args.GetReturnValue().Set(err);
}
#endif


void TCPWrap::Open(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  TCPWrap* wrap;
  UNWRAP(args.This(), TCPWrap, wrap);
  int fd = args[0]->IntegerValue();
  uv_tcp_open(&wrap->handle_, fd);
}


void TCPWrap::Bind(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap;
  UNWRAP(args.This(), TCPWrap, wrap);

  String::AsciiValue ip_address(args[0]);
  int port = args[1]->Int32Value();

  struct sockaddr_in address = uv_ip4_addr(*ip_address, port);
  int err = uv_tcp_bind(&wrap->handle_, address);

  args.GetReturnValue().Set(err);
}


void TCPWrap::Bind6(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap;
  UNWRAP(args.This(), TCPWrap, wrap);

  String::AsciiValue ip6_address(args[0]);
  int port = args[1]->Int32Value();

  struct sockaddr_in6 address = uv_ip6_addr(*ip6_address, port);
  int err = uv_tcp_bind6(&wrap->handle_, address);

  args.GetReturnValue().Set(err);
}


void TCPWrap::Listen(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap;
  UNWRAP(args.This(), TCPWrap, wrap);

  int backlog = args[0]->Int32Value();
  int err = uv_listen(reinterpret_cast<uv_stream_t*>(&wrap->handle_),
                      backlog,
                      OnConnection);
  args.GetReturnValue().Set(err);
}


void TCPWrap::OnConnection(uv_stream_t* handle, int status) {
  HandleScope scope(node_isolate);

  TCPWrap* tcp_wrap = static_cast<TCPWrap*>(handle->data);
  assert(&tcp_wrap->handle_ == reinterpret_cast<uv_tcp_t*>(handle));

  // We should not be getting this callback if someone as already called
  // uv_close() on the handle.
  assert(tcp_wrap->persistent().IsEmpty() == false);

  Local<Value> argv[2] = {
    Integer::New(status, node_isolate),
    Undefined()
  };

  if (status == 0) {
    // Instantiate the client javascript object and handle.
    Local<Object> client_obj = Instantiate();

    // Unwrap the client javascript object.
    TCPWrap* wrap;
    UNWRAP(client_obj, TCPWrap, wrap);
    uv_stream_t* client_handle = reinterpret_cast<uv_stream_t*>(&wrap->handle_);
    if (uv_accept(handle, client_handle))
      return;

    // Successful accept. Call the onconnection callback in JavaScript land.
    argv[1] = client_obj;
  }

  MakeCallback(tcp_wrap->object(), onconnection_sym, ARRAY_SIZE(argv), argv);
}


void TCPWrap::AfterConnect(uv_connect_t* req, int status) {
  ConnectWrap* req_wrap = static_cast<ConnectWrap*>(req->data);
  TCPWrap* wrap = static_cast<TCPWrap*>(req->handle->data);

  HandleScope scope(node_isolate);

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
  MakeCallback(req_wrap_obj, oncomplete_sym, ARRAY_SIZE(argv), argv);

  delete req_wrap;
}


void TCPWrap::Connect(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap;
  UNWRAP(args.This(), TCPWrap, wrap);

  assert(args[0]->IsObject());
  assert(args[1]->IsString());
  assert(args[2]->Uint32Value());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  String::AsciiValue ip_address(args[1]);
  int port = args[2]->Uint32Value();

  struct sockaddr_in address = uv_ip4_addr(*ip_address, port);

  // I hate when people program C++ like it was C, and yet I do it too.
  // I'm too lazy to come up with the perfect class hierarchy here. Let's
  // just do some type munging.
  ConnectWrap* req_wrap = new ConnectWrap(req_wrap_obj);

  int err = uv_tcp_connect(&req_wrap->req_,
                           &wrap->handle_,
                           address,
                           AfterConnect);
  req_wrap->Dispatched();
  if (err) delete req_wrap;

  args.GetReturnValue().Set(err);
}


void TCPWrap::Connect6(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  TCPWrap* wrap;
  UNWRAP(args.This(), TCPWrap, wrap);

  assert(args[0]->IsObject());
  assert(args[1]->IsString());
  assert(args[2]->Uint32Value());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  String::AsciiValue ip_address(args[1]);
  int port = args[2]->Int32Value();

  struct sockaddr_in6 address = uv_ip6_addr(*ip_address, port);

  ConnectWrap* req_wrap = new ConnectWrap(req_wrap_obj);

  int err = uv_tcp_connect6(&req_wrap->req_,
                            &wrap->handle_,
                            address,
                            AfterConnect);
  req_wrap->Dispatched();
  if (err) delete req_wrap;

  args.GetReturnValue().Set(err);
}


// also used by udp_wrap.cc
Local<Object> AddressToJS(const sockaddr* addr, Handle<Object> info) {
  static Cached<String> address_sym;
  static Cached<String> family_sym;
  static Cached<String> port_sym;
  static Cached<String> ipv4_sym;
  static Cached<String> ipv6_sym;

  HandleScope scope(node_isolate);
  char ip[INET6_ADDRSTRLEN];
  const sockaddr_in *a4;
  const sockaddr_in6 *a6;
  int port;

  if (address_sym.IsEmpty()) {
    address_sym = FIXED_ONE_BYTE_STRING(node_isolate, "address");
    family_sym = FIXED_ONE_BYTE_STRING(node_isolate, "family");
    port_sym = FIXED_ONE_BYTE_STRING(node_isolate, "port");
    ipv4_sym = FIXED_ONE_BYTE_STRING(node_isolate, "IPv4");
    ipv6_sym = FIXED_ONE_BYTE_STRING(node_isolate, "IPv6");
  }

  if (info.IsEmpty()) info = Object::New();

  switch (addr->sa_family) {
  case AF_INET6:
    a6 = reinterpret_cast<const sockaddr_in6*>(addr);
    uv_inet_ntop(AF_INET6, &a6->sin6_addr, ip, sizeof ip);
    port = ntohs(a6->sin6_port);
    info->Set(address_sym, OneByteString(node_isolate, ip));
    info->Set(family_sym, ipv6_sym);
    info->Set(port_sym, Integer::New(port, node_isolate));
    break;

  case AF_INET:
    a4 = reinterpret_cast<const sockaddr_in*>(addr);
    uv_inet_ntop(AF_INET, &a4->sin_addr, ip, sizeof ip);
    port = ntohs(a4->sin_port);
    info->Set(address_sym, OneByteString(node_isolate, ip));
    info->Set(family_sym, ipv4_sym);
    info->Set(port_sym, Integer::New(port, node_isolate));
    break;

  default:
    info->Set(address_sym, String::Empty(node_isolate));
  }

  return scope.Close(info);
}


}  // namespace node

NODE_MODULE(node_tcp_wrap, node::TCPWrap::Initialize)
