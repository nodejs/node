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

#include <stdlib.h>

// Temporary hack: libuv should provide uv_inet_pton and uv_inet_ntop.
// Clean this up in tcp_wrap.cc too.
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

using namespace v8;

namespace node {

#define UNWRAP                                                              \
  assert(!args.Holder().IsEmpty());                                         \
  assert(args.Holder()->InternalFieldCount() > 0);                          \
  UDPWrap* wrap =                                                           \
      static_cast<UDPWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
  if (!wrap) {                                                              \
    uv_err_t err;                                                           \
    err.code = UV_EBADF;                                                    \
    SetErrno(err);                                                          \
    return scope.Close(Integer::New(-1));                                   \
  }

// TODO share with tcp_wrap.cc
Persistent<String> address_symbol;
Persistent<String> port_symbol;
Persistent<String> buffer_sym;

void AddressToJS(Handle<Object> info,
                 const sockaddr* addr,
                 int addrlen);

typedef ReqWrap<uv_udp_send_t> SendWrap;

class UDPWrap: public HandleWrap {
public:
  static void Initialize(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Bind(const Arguments& args);
  static Handle<Value> Send(const Arguments& args);
  static Handle<Value> Bind6(const Arguments& args);
  static Handle<Value> Send6(const Arguments& args);
  static Handle<Value> RecvStart(const Arguments& args);
  static Handle<Value> RecvStop(const Arguments& args);
  static Handle<Value> GetSockName(const Arguments& args);

private:
  UDPWrap(Handle<Object> object);
  virtual ~UDPWrap();

  static Handle<Value> DoBind(const Arguments& args, int family);
  static Handle<Value> DoSend(const Arguments& args, int family);

  static uv_buf_t OnAlloc(uv_handle_t* handle, size_t suggested_size);
  static void OnSend(uv_udp_send_t* req, int status);
  static void OnRecv(uv_udp_t* handle,
                     ssize_t nread,
                     uv_buf_t buf,
                     struct sockaddr* addr,
                     unsigned flags);

  uv_udp_t handle_;
};


UDPWrap::UDPWrap(Handle<Object> object): HandleWrap(object,
                                                    (uv_handle_t*)&handle_) {
  int r = uv_udp_init(NODE_LOOP(), &handle_);
  assert(r == 0); // can't fail anyway
  handle_.data = reinterpret_cast<void*>(this);
}


UDPWrap::~UDPWrap() {
}


void UDPWrap::Initialize(Handle<Object> target) {
  HandleWrap::Initialize(target);

  HandleScope scope;

  buffer_sym = NODE_PSYMBOL("buffer");
  port_symbol = NODE_PSYMBOL("port");
  address_symbol = NODE_PSYMBOL("address");

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("UDP"));

  NODE_SET_PROTOTYPE_METHOD(t, "bind", Bind);
  NODE_SET_PROTOTYPE_METHOD(t, "send", Send);
  NODE_SET_PROTOTYPE_METHOD(t, "bind6", Bind6);
  NODE_SET_PROTOTYPE_METHOD(t, "send6", Send6);
  NODE_SET_PROTOTYPE_METHOD(t, "close", Close);
  NODE_SET_PROTOTYPE_METHOD(t, "recvStart", RecvStart);
  NODE_SET_PROTOTYPE_METHOD(t, "recvStop", RecvStop);
  NODE_SET_PROTOTYPE_METHOD(t, "getsockname", GetSockName);

  target->Set(String::NewSymbol("UDP"),
              Persistent<FunctionTemplate>::New(t)->GetFunction());
}


Handle<Value> UDPWrap::New(const Arguments& args) {
  HandleScope scope;

  assert(args.IsConstructCall());
  new UDPWrap(args.This());

  return scope.Close(args.This());
}


Handle<Value> UDPWrap::DoBind(const Arguments& args, int family) {
  HandleScope scope;
  int r;

  UNWRAP

  // bind(ip, port, flags)
  assert(args.Length() == 3);

  String::Utf8Value address(args[0]->ToString());
  const int port = args[1]->Uint32Value();
  const int flags = args[2]->Uint32Value();

  switch (family) {
  case AF_INET:
    r = uv_udp_bind(&wrap->handle_, uv_ip4_addr(*address, port), flags);
    break;
  case AF_INET6:
    r = uv_udp_bind6(&wrap->handle_, uv_ip6_addr(*address, port), flags);
    break;
  default:
    assert(0 && "unexpected address family");
    abort();
  }

  if (r)
    SetErrno(uv_last_error(NODE_LOOP()));

  return scope.Close(Integer::New(r));
}


Handle<Value> UDPWrap::Bind(const Arguments& args) {
  return DoBind(args, AF_INET);
}


Handle<Value> UDPWrap::Bind6(const Arguments& args) {
  return DoBind(args, AF_INET6);
}


Handle<Value> UDPWrap::DoSend(const Arguments& args, int family) {
  HandleScope scope;
  int r;

  // send(buffer, offset, length, port, address)
  assert(args.Length() == 5);

  UNWRAP

  assert(Buffer::HasInstance(args[0]));
  Local<Object> buffer_obj = args[0]->ToObject();

  size_t offset = args[1]->Uint32Value();
  size_t length = args[2]->Uint32Value();

  SendWrap* req_wrap = new SendWrap();
  req_wrap->object_->SetHiddenValue(buffer_sym, buffer_obj);

  uv_buf_t buf = uv_buf_init(Buffer::Data(buffer_obj) + offset,
                             length);

  const unsigned short port = args[3]->Uint32Value();
  String::Utf8Value address(args[4]->ToString());

  switch (family) {
  case AF_INET:
    r = uv_udp_send(&req_wrap->req_, &wrap->handle_, &buf, 1,
                    uv_ip4_addr(*address, port), OnSend);
    break;
  case AF_INET6:
    r = uv_udp_send6(&req_wrap->req_, &wrap->handle_, &buf, 1,
                     uv_ip6_addr(*address, port), OnSend);
    break;
  default:
    assert(0 && "unexpected address family");
    abort();
  }

  req_wrap->Dispatched();

  if (r) {
    SetErrno(uv_last_error(NODE_LOOP()));
    delete req_wrap;
    return Null();
  }
  else {
    return scope.Close(req_wrap->object_);
  }
}


Handle<Value> UDPWrap::Send(const Arguments& args) {
  return DoSend(args, AF_INET);
}


Handle<Value> UDPWrap::Send6(const Arguments& args) {
  return DoSend(args, AF_INET6);
}


Handle<Value> UDPWrap::RecvStart(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  // UV_EALREADY means that the socket is already bound but that's okay
  int r = uv_udp_recv_start(&wrap->handle_, OnAlloc, OnRecv);
  if (r && uv_last_error(NODE_LOOP()).code != UV_EALREADY) {
    SetErrno(uv_last_error(NODE_LOOP()));
    return False();
  }

  return True();
}


Handle<Value> UDPWrap::RecvStop(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  int r = uv_udp_recv_stop(&wrap->handle_);

  return scope.Close(Integer::New(r));
}


Handle<Value> UDPWrap::GetSockName(const Arguments& args) {
  HandleScope scope;
  struct sockaddr_storage address;

  UNWRAP

  int addrlen = sizeof(address);
  int r = uv_udp_getsockname(&wrap->handle_,
                             reinterpret_cast<sockaddr*>(&address),
                             &addrlen);

  if (r == 0) {
    Local<Object> sockname = Object::New();
    AddressToJS(sockname, reinterpret_cast<sockaddr*>(&address), addrlen);
    return scope.Close(sockname);
  }
  else {
    SetErrno(uv_last_error(NODE_LOOP()));
    return Null();
  }
}


// TODO share with StreamWrap::AfterWrite() in stream_wrap.cc
void UDPWrap::OnSend(uv_udp_send_t* req, int status) {
  HandleScope scope;

  assert(req != NULL);

  SendWrap* req_wrap = reinterpret_cast<SendWrap*>(req->data);
  UDPWrap* wrap = reinterpret_cast<UDPWrap*>(req->handle->data);

  assert(req_wrap->object_.IsEmpty() == false);
  assert(wrap->object_.IsEmpty() == false);

  if (status) {
    SetErrno(uv_last_error(NODE_LOOP()));
  }

  Local<Value> argv[4] = {
    Integer::New(status),
    Local<Value>::New(wrap->object_),
    Local<Value>::New(req_wrap->object_),
    req_wrap->object_->GetHiddenValue(buffer_sym),
  };

  MakeCallback(req_wrap->object_, "oncomplete", 4, argv);
  delete req_wrap;
}


uv_buf_t UDPWrap::OnAlloc(uv_handle_t* handle, size_t suggested_size) {
  // FIXME switch to slab allocation, share with stream_wrap.cc
  return uv_buf_init(new char[suggested_size], suggested_size);
}


static void ReleaseMemory(char* data, void* arg) {
  delete[] data; // data == buf.base
}


void UDPWrap::OnRecv(uv_udp_t* handle,
                     ssize_t nread,
                     uv_buf_t buf,
                     struct sockaddr* addr,
                     unsigned flags) {
  if (nread == 0) {
    ReleaseMemory(buf.base, NULL);
    return;
  }

  HandleScope scope;

  UDPWrap* wrap = reinterpret_cast<UDPWrap*>(handle->data);

  Handle<Value> argv[4] = {
    wrap->object_,
    Integer::New(nread),
    Null(),
    Null()
  };

  if (nread == -1) {
    SetErrno(uv_last_error(NODE_LOOP()));
  }
  else {
    Local<Object> rinfo = Object::New();
    AddressToJS(rinfo, addr, sizeof *addr);
    argv[2] = Buffer::New(buf.base, nread, ReleaseMemory, NULL)->handle_;
    argv[3] = rinfo;
  }

  MakeCallback(wrap->object_, "onmessage", ARRAY_SIZE(argv), argv);
}


void AddressToJS(Handle<Object> info,
                 const sockaddr* addr,
                 int addrlen) {
  char ip[INET6_ADDRSTRLEN];
  const sockaddr_in *a4;
  const sockaddr_in6 *a6;
  int port;

  assert(addr != NULL);

  if (addrlen == 0) {
    info->Set(address_symbol, String::Empty());
    return;
  }

  switch (addr->sa_family) {
  case AF_INET6:
    a6 = reinterpret_cast<const sockaddr_in6*>(addr);
    uv_inet_ntop(AF_INET6, &a6->sin6_addr, ip, sizeof ip);
    port = ntohs(a6->sin6_port);
    info->Set(address_symbol, String::New(ip));
    info->Set(port_symbol, Integer::New(port));
    break;

  case AF_INET:
    a4 = reinterpret_cast<const sockaddr_in*>(addr);
    uv_inet_ntop(AF_INET, &a4->sin_addr, ip, sizeof ip);
    port = ntohs(a4->sin_port);
    info->Set(address_symbol, String::New(ip));
    info->Set(port_symbol, Integer::New(port));
    break;

  default:
    info->Set(address_symbol, String::Empty());
  }
}


} // namespace node

NODE_MODULE(node_udp_wrap, node::UDPWrap::Initialize)
