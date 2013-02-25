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
#include "slab_allocator.h"
#include "req_wrap.h"
#include "handle_wrap.h"
#include "udp_wrap.h"

#include <stdlib.h>

#define SLAB_SIZE (1024 * 1024)


namespace node {

using v8::AccessorInfo;
using v8::Arguments;
using v8::False;
using v8::Function;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Null;
using v8::Object;
using v8::Persistent;
using v8::PropertyAttribute;
using v8::String;
using v8::True;
using v8::Value;

typedef ReqWrap<uv_udp_send_t> SendWrap;

// see tcp_wrap.cc
Local<Object> AddressToJS(const sockaddr* addr);

static Persistent<Function> constructor;
static Persistent<String> buffer_sym;
static Persistent<String> oncomplete_sym;
static Persistent<String> onmessage_sym;
static SlabAllocator* slab_allocator;


static void DeleteSlabAllocator(void*) {
  delete slab_allocator;
  slab_allocator = NULL;
}


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

  slab_allocator = new SlabAllocator(SLAB_SIZE);
  AtExit(DeleteSlabAllocator, NULL);

  HandleScope scope;

  buffer_sym = NODE_PSYMBOL("buffer");
  oncomplete_sym = NODE_PSYMBOL("oncomplete");
  onmessage_sym = NODE_PSYMBOL("onmessage");

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("UDP"));

  enum PropertyAttribute attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  t->InstanceTemplate()->SetAccessor(String::New("fd"),
                                     UDPWrap::GetFD,
                                     NULL,
                                     Handle<Value>(),
                                     v8::DEFAULT,
                                     attributes);

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

  NODE_SET_PROTOTYPE_METHOD(t, "ref", HandleWrap::Ref);
  NODE_SET_PROTOTYPE_METHOD(t, "unref", HandleWrap::Unref);

  constructor = Persistent<Function>::New(
      Persistent<FunctionTemplate>::New(t)->GetFunction());
  target->Set(String::NewSymbol("UDP"), constructor);
}


Handle<Value> UDPWrap::New(const Arguments& args) {
  HandleScope scope;

  assert(args.IsConstructCall());
  new UDPWrap(args.This());

  return scope.Close(args.This());
}


Handle<Value> UDPWrap::GetFD(Local<String>, const AccessorInfo& args) {
#if defined(_WIN32)
  return v8::Null();
#else
  HandleScope scope;
  UNWRAP(UDPWrap)
  int fd = (wrap == NULL) ? -1 : wrap->handle_.io_watcher.fd;
  return scope.Close(Integer::New(fd));
#endif
}


Handle<Value> UDPWrap::DoBind(const Arguments& args, int family) {
  HandleScope scope;
  int r;

  UNWRAP(UDPWrap)

  // bind(ip, port, flags)
  assert(args.Length() == 3);

  String::Utf8Value address(args[0]);
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
    UNWRAP(UDPWrap)                                                           \
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
  UNWRAP(UDPWrap)

  assert(args.Length() == 2);

  String::Utf8Value address(args[0]);
  String::Utf8Value iface(args[1]);

  const char* iface_cstr = *iface;
  if (args[1]->IsUndefined() || args[1]->IsNull()) {
      iface_cstr = NULL;
  }

  int r = uv_udp_set_membership(&wrap->handle_, *address, iface_cstr,
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

  UNWRAP(UDPWrap)

  assert(Buffer::HasInstance(args[0]));
  Local<Object> buffer_obj = args[0]->ToObject();

  size_t offset = args[1]->Uint32Value();
  size_t length = args[2]->Uint32Value();
  assert(offset < Buffer::Length(buffer_obj));
  assert(length <= Buffer::Length(buffer_obj) - offset);

  SendWrap* req_wrap = new SendWrap();
  req_wrap->object_->SetHiddenValue(buffer_sym, buffer_obj);

  uv_buf_t buf = uv_buf_init(Buffer::Data(buffer_obj) + offset,
                             length);

  const unsigned short port = args[3]->Uint32Value();
  String::Utf8Value address(args[4]);

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

  UNWRAP(UDPWrap)

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

  UNWRAP(UDPWrap)

  int r = uv_udp_recv_stop(&wrap->handle_);

  return scope.Close(Integer::New(r));
}


Handle<Value> UDPWrap::GetSockName(const Arguments& args) {
  HandleScope scope;
  struct sockaddr_storage address;

  UNWRAP(UDPWrap)

  int addrlen = sizeof(address);
  int r = uv_udp_getsockname(&wrap->handle_,
                             reinterpret_cast<sockaddr*>(&address),
                             &addrlen);

  if (r) {
    SetErrno(uv_last_error(uv_default_loop()));
    return Null();
  }

  const sockaddr* addr = reinterpret_cast<const sockaddr*>(&address);
  return scope.Close(AddressToJS(addr));
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

  MakeCallback(req_wrap->object_, oncomplete_sym, ARRAY_SIZE(argv), argv);
  delete req_wrap;
}


uv_buf_t UDPWrap::OnAlloc(uv_handle_t* handle, size_t suggested_size) {
  UDPWrap* wrap = static_cast<UDPWrap*>(handle->data);
  char* buf = slab_allocator->Allocate(wrap->object_, suggested_size);
  return uv_buf_init(buf, suggested_size);
}


void UDPWrap::OnRecv(uv_udp_t* handle,
                     ssize_t nread,
                     uv_buf_t buf,
                     struct sockaddr* addr,
                     unsigned flags) {
  HandleScope scope;

  UDPWrap* wrap = reinterpret_cast<UDPWrap*>(handle->data);
  Local<Object> slab = slab_allocator->Shrink(wrap->object_,
                                              buf.base,
                                              nread < 0 ? 0 : nread);
  if (nread == 0) return;

  if (nread < 0) {
    Local<Value> argv[] = { Local<Object>::New(wrap->object_) };
    SetErrno(uv_last_error(uv_default_loop()));
    MakeCallback(wrap->object_, onmessage_sym, ARRAY_SIZE(argv), argv);
    return;
  }

  Local<Value> argv[] = {
    Local<Object>::New(wrap->object_),
    slab,
    Integer::NewFromUnsigned(buf.base - Buffer::Data(slab)),
    Integer::NewFromUnsigned(nread),
    AddressToJS(addr)
  };
  MakeCallback(wrap->object_, onmessage_sym, ARRAY_SIZE(argv), argv);
}


UDPWrap* UDPWrap::Unwrap(Local<Object> obj) {
  assert(!obj.IsEmpty());
  assert(obj->InternalFieldCount() > 0);
  return static_cast<UDPWrap*>(obj->GetPointerFromInternalField(0));
}


Local<Object> UDPWrap::Instantiate() {
  // If this assert fires then Initialize hasn't been called yet.
  assert(constructor.IsEmpty() == false);

  HandleScope scope;
  Local<Object> obj = constructor->NewInstance();

  return scope.Close(obj);
}


uv_udp_t* UDPWrap::UVHandle() {
  return &handle_;
}


} // namespace node

NODE_MODULE(node_udp_wrap, node::UDPWrap::Initialize)
