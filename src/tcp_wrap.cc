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
#include <node_buffer.h>
#include <req_wrap.h>
#include <handle_wrap.h>
#include <stream_wrap.h>
#include <tcp_wrap.h>

#include <stdlib.h>

// Temporary hack: libuv should provide uv_inet_pton and uv_inet_ntop.
#if defined(__MINGW32__) || defined(_MSC_VER)
  extern "C" {
#   include <inet_net_pton.h>
#   include <inet_ntop.h>
  }
# define uv_inet_pton ares_inet_pton
# define uv_inet_ntop ares_inet_ntop

#else // __POSIX__
# include <arpa/inet.h>
# define uv_inet_pton inet_pton
# define uv_inet_ntop inet_ntop
#endif

#define UNWRAP \
  assert(!args.Holder().IsEmpty()); \
  assert(args.Holder()->InternalFieldCount() > 0); \
  TCPWrap* wrap =  \
      static_cast<TCPWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
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
using v8::Undefined;

static Persistent<Function> tcpConstructor;
static Persistent<String> family_symbol;
static Persistent<String> address_symbol;
static Persistent<String> port_symbol;


typedef class ReqWrap<uv_connect_t> ConnectWrap;


Local<Object> TCPWrap::Instantiate() {
  // If this assert fire then process.binding('tcp_wrap') hasn't been
  // called yet.
  assert(tcpConstructor.IsEmpty() == false);

  HandleScope scope;
  Local<Object> obj = tcpConstructor->NewInstance();

  return scope.Close(obj);
}


void TCPWrap::Initialize(Handle<Object> target) {
  HandleWrap::Initialize(target);
  StreamWrap::Initialize(target);

  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  t->SetClassName(String::NewSymbol("TCP"));

  t->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(t, "close", HandleWrap::Close);

  NODE_SET_PROTOTYPE_METHOD(t, "readStart", StreamWrap::ReadStart);
  NODE_SET_PROTOTYPE_METHOD(t, "readStop", StreamWrap::ReadStop);
  NODE_SET_PROTOTYPE_METHOD(t, "write", StreamWrap::Write);
  NODE_SET_PROTOTYPE_METHOD(t, "shutdown", StreamWrap::Shutdown);

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
  NODE_SET_PROTOTYPE_METHOD(t, "setSimultaneousAccepts", SetSimultaneousAccepts);
#endif

  tcpConstructor = Persistent<Function>::New(t->GetFunction());

  family_symbol = NODE_PSYMBOL("family");
  address_symbol = NODE_PSYMBOL("address");
  port_symbol = NODE_PSYMBOL("port");

  target->Set(String::NewSymbol("TCP"), tcpConstructor);
}


Handle<Value> TCPWrap::New(const Arguments& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  assert(args.IsConstructCall());

  HandleScope scope;
  TCPWrap* wrap = new TCPWrap(args.This());
  assert(wrap);

  return scope.Close(args.This());
}


TCPWrap::TCPWrap(Handle<Object> object)
    : StreamWrap(object, (uv_stream_t*) &handle_) {
  int r = uv_tcp_init(uv_default_loop(), &handle_);
  assert(r == 0); // How do we proxy this error up to javascript?
                  // Suggestion: uv_tcp_init() returns void.
  UpdateWriteQueueSize();
}


TCPWrap::~TCPWrap() {
  assert(object_.IsEmpty());
}


Handle<Value> TCPWrap::GetSockName(const Arguments& args) {
  HandleScope scope;
  struct sockaddr_storage address;
  int family;
  int port;
  char ip[INET6_ADDRSTRLEN];

  UNWRAP

  int addrlen = sizeof(address);
  int r = uv_tcp_getsockname(&wrap->handle_,
                             reinterpret_cast<sockaddr*>(&address),
                             &addrlen);

  Local<Object> sockname = Object::New();
  if (r != 0) {
    SetErrno(uv_last_error(uv_default_loop()));
  } else {
    family = address.ss_family;

    if (family == AF_INET) {
      struct sockaddr_in* addrin = (struct sockaddr_in*)&address;
      uv_inet_ntop(AF_INET, &(addrin->sin_addr), ip, INET6_ADDRSTRLEN);
      port = ntohs(addrin->sin_port);
    } else if (family == AF_INET6) {
      struct sockaddr_in6* addrin6 = (struct sockaddr_in6*)&address;
      uv_inet_ntop(AF_INET6, &(addrin6->sin6_addr), ip, INET6_ADDRSTRLEN);
      port = ntohs(addrin6->sin6_port);
    } else {
      assert(0 && "bad address family");
      abort();
    }

    sockname->Set(port_symbol, Integer::New(port));
    sockname->Set(family_symbol, Integer::New(family));
    sockname->Set(address_symbol, String::New(ip));
  }

  return scope.Close(sockname);
}


Handle<Value> TCPWrap::GetPeerName(const Arguments& args) {
  HandleScope scope;
  struct sockaddr_storage address;
  int family;
  int port;
  char ip[INET6_ADDRSTRLEN];

  UNWRAP

  int addrlen = sizeof(address);
  int r = uv_tcp_getpeername(&wrap->handle_,
                             reinterpret_cast<sockaddr*>(&address),
                             &addrlen);

  Local<Object> sockname = Object::New();
  if (r != 0) {
    SetErrno(uv_last_error(uv_default_loop()));
  } else {
    family = address.ss_family;

    if (family == AF_INET) {
      struct sockaddr_in* addrin = (struct sockaddr_in*)&address;
      uv_inet_ntop(AF_INET, &(addrin->sin_addr), ip, INET6_ADDRSTRLEN);
      port = ntohs(addrin->sin_port);
    } else if (family == AF_INET6) {
      struct sockaddr_in6* addrin6 = (struct sockaddr_in6*)&address;
      uv_inet_ntop(AF_INET6, &(addrin6->sin6_addr), ip, INET6_ADDRSTRLEN);
      port = ntohs(addrin6->sin6_port);
    } else {
      assert(0 && "bad address family");
      abort();
    }

    sockname->Set(port_symbol, Integer::New(port));
    sockname->Set(family_symbol, Integer::New(family));
    sockname->Set(address_symbol, String::New(ip));
  }

  return scope.Close(sockname);
}


Handle<Value> TCPWrap::SetNoDelay(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  int r = uv_tcp_nodelay(&wrap->handle_, 1);
  if (r)
    SetErrno(uv_last_error(uv_default_loop()));

  return Undefined();
}


Handle<Value> TCPWrap::SetKeepAlive(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  int enable = args[0]->Int32Value();
  unsigned int delay = args[1]->Uint32Value();

  int r = uv_tcp_keepalive(&wrap->handle_, enable, delay);
  if (r)
    SetErrno(uv_last_error(uv_default_loop()));

  return Undefined();
}


#ifdef _WIN32
Handle<Value> TCPWrap::SetSimultaneousAccepts(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  bool enable = args[0]->BooleanValue();

  int r = uv_tcp_simultaneous_accepts(&wrap->handle_, enable ? 1 : 0);
  if (r)
    SetErrno(uv_last_error(uv_default_loop()));

  return Undefined();
}
#endif


Handle<Value> TCPWrap::Bind(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  String::AsciiValue ip_address(args[0]->ToString());
  int port = args[1]->Int32Value();

  struct sockaddr_in address = uv_ip4_addr(*ip_address, port);
  int r = uv_tcp_bind(&wrap->handle_, address);

  // Error starting the tcp.
  if (r) SetErrno(uv_last_error(uv_default_loop()));

  return scope.Close(Integer::New(r));
}


Handle<Value> TCPWrap::Bind6(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  String::AsciiValue ip6_address(args[0]->ToString());
  int port = args[1]->Int32Value();

  struct sockaddr_in6 address = uv_ip6_addr(*ip6_address, port);
  int r = uv_tcp_bind6(&wrap->handle_, address);

  // Error starting the tcp.
  if (r) SetErrno(uv_last_error(uv_default_loop()));

  return scope.Close(Integer::New(r));
}


Handle<Value> TCPWrap::Listen(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  int backlog = args[0]->Int32Value();

  int r = uv_listen((uv_stream_t*)&wrap->handle_, backlog, OnConnection);

  // Error starting the tcp.
  if (r) SetErrno(uv_last_error(uv_default_loop()));

  return scope.Close(Integer::New(r));
}


void TCPWrap::OnConnection(uv_stream_t* handle, int status) {
  HandleScope scope;

  TCPWrap* wrap = static_cast<TCPWrap*>(handle->data);
  assert(&wrap->handle_ == (uv_tcp_t*)handle);

  // We should not be getting this callback if someone as already called
  // uv_close() on the handle.
  assert(wrap->object_.IsEmpty() == false);

  Handle<Value> argv[1];

  if (status == 0) {
    // Instantiate the client javascript object and handle.
    Local<Object> client_obj = Instantiate();

    // Unwrap the client javascript object.
    assert(client_obj->InternalFieldCount() > 0);
    TCPWrap* client_wrap =
        static_cast<TCPWrap*>(client_obj->GetPointerFromInternalField(0));

    if (uv_accept(handle, (uv_stream_t*)&client_wrap->handle_)) return;

    // Successful accept. Call the onconnection callback in JavaScript land.
    argv[0] = client_obj;
  } else {
    SetErrno(uv_last_error(uv_default_loop()));
    argv[0] = v8::Null();
  }

  MakeCallback(wrap->object_, "onconnection", 1, argv);
}


void TCPWrap::AfterConnect(uv_connect_t* req, int status) {
  ConnectWrap* req_wrap = (ConnectWrap*) req->data;
  TCPWrap* wrap = (TCPWrap*) req->handle->data;

  HandleScope scope;

  // The wrap and request objects should still be there.
  assert(req_wrap->object_.IsEmpty() == false);
  assert(wrap->object_.IsEmpty() == false);

  if (status) {
    SetErrno(uv_last_error(uv_default_loop()));
  }

  Local<Value> argv[5] = {
    Integer::New(status),
    Local<Value>::New(wrap->object_),
    Local<Value>::New(req_wrap->object_),
    Local<Value>::New(v8::True()),
    Local<Value>::New(v8::True())
  };

  MakeCallback(req_wrap->object_, "oncomplete", 5, argv);

  delete req_wrap;
}


Handle<Value> TCPWrap::Connect(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  String::AsciiValue ip_address(args[0]->ToString());
  int port = args[1]->Int32Value();

  struct sockaddr_in address = uv_ip4_addr(*ip_address, port);

  // I hate when people program C++ like it was C, and yet I do it too.
  // I'm too lazy to come up with the perfect class hierarchy here. Let's
  // just do some type munging.
  ConnectWrap* req_wrap = new ConnectWrap();

  int r = uv_tcp_connect(&req_wrap->req_, &wrap->handle_, address,
      AfterConnect);

  req_wrap->Dispatched();

  if (r) {
    SetErrno(uv_last_error(uv_default_loop()));
    delete req_wrap;
    return scope.Close(v8::Null());
  } else {
    return scope.Close(req_wrap->object_);
  }
}


Handle<Value> TCPWrap::Connect6(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  String::AsciiValue ip_address(args[0]->ToString());
  int port = args[1]->Int32Value();

  struct sockaddr_in6 address = uv_ip6_addr(*ip_address, port);

  ConnectWrap* req_wrap = new ConnectWrap();

  int r = uv_tcp_connect6(&req_wrap->req_, &wrap->handle_, address,
      AfterConnect);

  req_wrap->Dispatched();

  if (r) {
    SetErrno(uv_last_error(uv_default_loop()));
    delete req_wrap;
    return scope.Close(v8::Null());
  } else {
    return scope.Close(req_wrap->object_);
  }
}


}  // namespace node

NODE_MODULE(node_tcp_wrap, node::TCPWrap::Initialize)
