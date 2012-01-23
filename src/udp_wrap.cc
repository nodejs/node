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

#define SLAB_SIZE (1024 * 1024)
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// TODO share with tcp_wrap.cc
Persistent<String> address_symbol;
Persistent<String> port_symbol;
Persistent<String> buffer_sym;
static Persistent<String> udp_slab_sym;

void AddressToJS(Handle<Object> info,
                 const sockaddr* addr,
                 int addrlen);

typedef ReqWrap<uv_udp_send_t> SendWrap;


static size_t slab_used;
size_t slab_offset_;
static uv_handle_t* handle_that_last_alloced;


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
  static Handle<Value> AddMembership(const Arguments& args);
  static Handle<Value> DropMembership(const Arguments& args);
  static Handle<Value> SetMulticastTTL(const Arguments& args);
  static Handle<Value> SetMulticastLoopback(const Arguments& args);
  static Handle<Value> SetBroadcast(const Arguments& args);
  static Handle<Value> SetTTL(const Arguments& args);

private:
  static inline char* NewSlab(v8::Handle<v8::Object> global, v8::Handle<v8::Object> wrap_obj);

  UDPWrap(Handle<Object> object);
  virtual ~UDPWrap();

  static Handle<Value> DoBind(const Arguments& args, int family);
  static Handle<Value> DoSend(const Arguments& args, int family);
  static Handle<Value> SetMembership(const Arguments& args,
                                     uv_membership membership);

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
  int r = uv_udp_init(uv_default_loop(), &handle_);
  assert(r == 0); // can't fail anyway
  handle_.data = reinterpret_cast<void*>(this);
}


UDPWrap::~UDPWrap() {
}


void UDPWrap::Initialize(Handle<Object> target) {
  HandleWrap::Initialize(target);

  HandleScope scope;

  udp_slab_sym = Persistent<String>::New(String::NewSymbol("udpslab"));
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
  NODE_SET_PROTOTYPE_METHOD(t, "addMembership", AddMembership);
  NODE_SET_PROTOTYPE_METHOD(t, "dropMembership", DropMembership);
  NODE_SET_PROTOTYPE_METHOD(t, "setMulticastTTL", SetMulticastTTL);
  NODE_SET_PROTOTYPE_METHOD(t, "setMulticastLoopback", SetMulticastLoopback);
  NODE_SET_PROTOTYPE_METHOD(t, "setBroadcast", SetBroadcast);
  NODE_SET_PROTOTYPE_METHOD(t, "setTTL", SetTTL);

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
    SetErrno(uv_last_error(uv_default_loop()));

  return scope.Close(Integer::New(r));
}


Handle<Value> UDPWrap::Bind(const Arguments& args) {
  return DoBind(args, AF_INET);
}


Handle<Value> UDPWrap::Bind6(const Arguments& args) {
  return DoBind(args, AF_INET6);
}


#define X(name, fn)                                                           \
  Handle<Value> UDPWrap::name(const Arguments& args) {                        \
    HandleScope scope;                                                        \
    UNWRAP                                                                    \
    assert(args.Length() == 1);                                               \
    int flag = args[0]->Int32Value();                                         \
    int r = fn(&wrap->handle_, flag);                                         \
    if (r) SetErrno(uv_last_error(uv_default_loop()));                        \
    return scope.Close(Integer::New(r));                                      \
  }

X(SetTTL, uv_udp_set_ttl)
X(SetBroadcast, uv_udp_set_broadcast)
X(SetMulticastTTL, uv_udp_set_multicast_ttl)
X(SetMulticastLoopback, uv_udp_set_multicast_loop)

#undef X


Handle<Value> UDPWrap::SetMembership(const Arguments& args,
                                     uv_membership membership) {
  HandleScope scope;
  UNWRAP

  assert(args.Length() == 2);

  String::Utf8Value address(args[0]->ToString());
  String::Utf8Value interface(args[1]->ToString());

  const char* interface_cstr = *interface;
  if (args[1]->IsUndefined() || args[1]->IsNull()) {
      interface_cstr = NULL;
  }

  int r = uv_udp_set_membership(&wrap->handle_, *address, interface_cstr,
                                membership);

  if (r)
    SetErrno(uv_last_error(uv_default_loop()));

  return scope.Close(Integer::New(r));
}


Handle<Value> UDPWrap::AddMembership(const Arguments& args) {
  return SetMembership(args, UV_JOIN_GROUP);
}


Handle<Value> UDPWrap::DropMembership(const Arguments& args) {
  return SetMembership(args, UV_LEAVE_GROUP);
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
    SetErrno(uv_last_error(uv_default_loop()));
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
  if (r && uv_last_error(uv_default_loop()).code != UV_EALREADY) {
    SetErrno(uv_last_error(uv_default_loop()));
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
    SetErrno(uv_last_error(uv_default_loop()));
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
    SetErrno(uv_last_error(uv_default_loop()));
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
  HandleScope scope;

  UDPWrap* wrap = static_cast<UDPWrap*>(handle->data);

  char* slab = NULL;

  Handle<Object> global = Context::GetCurrent()->Global();
  Local<Value> slab_v = global->GetHiddenValue(udp_slab_sym);

  if (slab_v.IsEmpty()) {
    // No slab currently. Create a new one.
    slab = NewSlab(global, wrap->object_);
  } else {
    // Use existing slab.
    Local<Object> slab_obj = slab_v->ToObject();
    slab = Buffer::Data(slab_obj);
    assert(Buffer::Length(slab_obj) == SLAB_SIZE);
    assert(SLAB_SIZE >= slab_used);

    // If less than 64kb is remaining on the slab allocate a new one.
    if (SLAB_SIZE - slab_used < 64 * 1024) {
      slab = NewSlab(global, wrap->object_);
    } else {
      wrap->object_->SetHiddenValue(udp_slab_sym, slab_obj);
    }
  }

  uv_buf_t buf;
  buf.base = slab + slab_used;
  buf.len = MIN(SLAB_SIZE - slab_used, suggested_size);

  slab_offset_ = slab_used;
  slab_used += buf.len;

  handle_that_last_alloced = reinterpret_cast<uv_handle_t*>(handle);

  return buf;

}


void UDPWrap::OnRecv(uv_udp_t* handle,
                     ssize_t nread,
                     uv_buf_t buf,
                     struct sockaddr* addr,
                     unsigned flags) {
  if (nread == 0) {
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
    SetErrno(uv_last_error(uv_default_loop()));
  }
  else {
    Local<Object> rinfo = Object::New();
    AddressToJS(rinfo, addr, sizeof *addr);
    argv[2] = Buffer::New(buf.base, nread, NULL, NULL)->handle_;
    argv[3] = rinfo;
  }

  MakeCallback(wrap->object_, "onmessage", ARRAY_SIZE(argv), argv);
}

inline char* UDPWrap::NewSlab(Handle<Object> global,
                                        Handle<Object> wrap_obj) {
  Buffer* b = Buffer::New(SLAB_SIZE);
  global->SetHiddenValue(udp_slab_sym, b->handle_);
  assert(Buffer::Length(b) == SLAB_SIZE);
  slab_used = 0;
  wrap_obj->SetHiddenValue(udp_slab_sym, b->handle_);
  return Buffer::Data(b);
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
