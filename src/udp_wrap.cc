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

#include "udp_wrap.h"
#include "node.h"
#include "node_buffer.h"
#include "handle_wrap.h"
#include "req_wrap.h"

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
using v8::PropertyCallbackInfo;
using v8::String;
using v8::Uint32;
using v8::Undefined;
using v8::Value;

typedef ReqWrap<uv_udp_send_t> SendWrap;

static Persistent<Function> constructor;
static Cached<String> buffer_sym;
static Cached<String> oncomplete_sym;
static Cached<String> onmessage_sym;


UDPWrap::UDPWrap(Handle<Object> object)
    : HandleWrap(object, reinterpret_cast<uv_handle_t*>(&handle_)) {
  int r = uv_udp_init(uv_default_loop(), &handle_);
  assert(r == 0);  // can't fail anyway
}


UDPWrap::~UDPWrap() {
}


void UDPWrap::Initialize(Handle<Object> target) {
  HandleWrap::Initialize(target);

  HandleScope scope(node_isolate);

  buffer_sym = String::New("buffer");
  oncomplete_sym = String::New("oncomplete");
  onmessage_sym = String::New("onmessage");

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

  constructor.Reset(node_isolate, t->GetFunction());
  target->Set(String::NewSymbol("UDP"), t->GetFunction());
}


void UDPWrap::New(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  assert(args.IsConstructCall());
  new UDPWrap(args.This());
}


void UDPWrap::GetFD(Local<String>, const PropertyCallbackInfo<Value>& args) {
#if !defined(_WIN32)
  HandleScope scope(node_isolate);
  UNWRAP(UDPWrap)
  int fd = (wrap == NULL) ? -1 : wrap->handle_.io_watcher.fd;
  args.GetReturnValue().Set(fd);
#endif
}


void UDPWrap::DoBind(const FunctionCallbackInfo<Value>& args, int family) {
  HandleScope scope(node_isolate);
  int err;

  UNWRAP(UDPWrap)

  // bind(ip, port, flags)
  assert(args.Length() == 3);

  String::Utf8Value address(args[0]);
  const int port = args[1]->Uint32Value();
  const int flags = args[2]->Uint32Value();

  switch (family) {
  case AF_INET:
    err = uv_udp_bind(&wrap->handle_, uv_ip4_addr(*address, port), flags);
    break;
  case AF_INET6:
    err = uv_udp_bind6(&wrap->handle_, uv_ip6_addr(*address, port), flags);
    break;
  default:
    assert(0 && "unexpected address family");
    abort();
  }

  args.GetReturnValue().Set(err);
}


void UDPWrap::Bind(const FunctionCallbackInfo<Value>& args) {
  DoBind(args, AF_INET);
}


void UDPWrap::Bind6(const FunctionCallbackInfo<Value>& args) {
  DoBind(args, AF_INET6);
}


#define X(name, fn)                                                           \
  void UDPWrap::name(const FunctionCallbackInfo<Value>& args) {               \
    HandleScope scope(node_isolate);                                          \
    UNWRAP(UDPWrap)                                                           \
    assert(args.Length() == 1);                                               \
    int flag = args[0]->Int32Value();                                         \
    int err = fn(&wrap->handle_, flag);                                       \
    args.GetReturnValue().Set(err);                                           \
  }

X(SetTTL, uv_udp_set_ttl)
X(SetBroadcast, uv_udp_set_broadcast)
X(SetMulticastTTL, uv_udp_set_multicast_ttl)
X(SetMulticastLoopback, uv_udp_set_multicast_loop)

#undef X


void UDPWrap::SetMembership(const FunctionCallbackInfo<Value>& args,
                            uv_membership membership) {
  HandleScope scope(node_isolate);
  UNWRAP(UDPWrap)

  assert(args.Length() == 2);

  String::Utf8Value address(args[0]);
  String::Utf8Value iface(args[1]);

  const char* iface_cstr = *iface;
  if (args[1]->IsUndefined() || args[1]->IsNull()) {
      iface_cstr = NULL;
  }

  int err = uv_udp_set_membership(&wrap->handle_,
                                  *address,
                                  iface_cstr,
                                  membership);
  args.GetReturnValue().Set(err);
}


void UDPWrap::AddMembership(const FunctionCallbackInfo<Value>& args) {
  SetMembership(args, UV_JOIN_GROUP);
}


void UDPWrap::DropMembership(const FunctionCallbackInfo<Value>& args) {
  SetMembership(args, UV_LEAVE_GROUP);
}


void UDPWrap::DoSend(const FunctionCallbackInfo<Value>& args, int family) {
  HandleScope scope(node_isolate);
  int err;

  UNWRAP(UDPWrap)

  // send(req, buffer, offset, length, port, address)
  assert(args[0]->IsObject());
  assert(Buffer::HasInstance(args[1]));
  assert(args[2]->IsUint32());
  assert(args[3]->IsUint32());
  assert(args[4]->IsUint32());
  assert(args[5]->IsString());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  Local<Object> buffer_obj = args[1].As<Object>();
  size_t offset = args[2]->Uint32Value();
  size_t length = args[3]->Uint32Value();
  const unsigned short port = args[4]->Uint32Value();
  String::Utf8Value address(args[5]);

  assert(offset < Buffer::Length(buffer_obj));
  assert(length <= Buffer::Length(buffer_obj) - offset);

  SendWrap* req_wrap = new SendWrap(req_wrap_obj);
  req_wrap->object()->SetHiddenValue(buffer_sym, buffer_obj);

  uv_buf_t buf = uv_buf_init(Buffer::Data(buffer_obj) + offset,
                             length);

  switch (family) {
  case AF_INET:
    err = uv_udp_send(&req_wrap->req_,
                      &wrap->handle_,
                      &buf,
                      1,
                      uv_ip4_addr(*address, port),
                      OnSend);
    break;
  case AF_INET6:
    err = uv_udp_send6(&req_wrap->req_,
                       &wrap->handle_,
                       &buf,
                       1,
                       uv_ip6_addr(*address, port),
                       OnSend);
    break;
  default:
    assert(0 && "unexpected address family");
    abort();
  }

  req_wrap->Dispatched();
  if (err) delete req_wrap;

  args.GetReturnValue().Set(err);
}


void UDPWrap::Send(const FunctionCallbackInfo<Value>& args) {
  DoSend(args, AF_INET);
}


void UDPWrap::Send6(const FunctionCallbackInfo<Value>& args) {
  DoSend(args, AF_INET6);
}


void UDPWrap::RecvStart(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  UNWRAP(UDPWrap)

  int err = uv_udp_recv_start(&wrap->handle_, OnAlloc, OnRecv);
  // UV_EALREADY means that the socket is already bound but that's okay
  if (err == UV_EALREADY) err = 0;
  args.GetReturnValue().Set(err);
}


void UDPWrap::RecvStop(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  UNWRAP(UDPWrap)

  int r = uv_udp_recv_stop(&wrap->handle_);
  args.GetReturnValue().Set(r);
}


void UDPWrap::GetSockName(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  struct sockaddr_storage address;
  UNWRAP(UDPWrap)

  assert(args[0]->IsObject());
  Local<Object> obj = args[0].As<Object>();

  int addrlen = sizeof(address);
  int err = uv_udp_getsockname(&wrap->handle_,
                               reinterpret_cast<sockaddr*>(&address),
                               &addrlen);

  if (err == 0) {
    const sockaddr* addr = reinterpret_cast<const sockaddr*>(&address);
    AddressToJS(addr, obj);
  }

  args.GetReturnValue().Set(err);
}


// TODO(bnoordhuis) share with StreamWrap::AfterWrite() in stream_wrap.cc
void UDPWrap::OnSend(uv_udp_send_t* req, int status) {
  HandleScope scope(node_isolate);

  assert(req != NULL);

  SendWrap* req_wrap = reinterpret_cast<SendWrap*>(req->data);
  UDPWrap* wrap = reinterpret_cast<UDPWrap*>(req->handle->data);

  assert(req_wrap->persistent().IsEmpty() == false);
  assert(wrap->persistent().IsEmpty() == false);

  Local<Object> req_wrap_obj = req_wrap->object();
  Local<Value> arg = Integer::New(status, node_isolate);
  MakeCallback(req_wrap_obj, oncomplete_sym, 1, &arg);
  delete req_wrap;
}


uv_buf_t UDPWrap::OnAlloc(uv_handle_t* handle, size_t suggested_size) {
  char* data = static_cast<char*>(malloc(suggested_size));
  if (data == NULL && suggested_size > 0) {
    FatalError("node::UDPWrap::OnAlloc(uv_handle_t*, size_t)",
               "Out Of Memory");
  }
  return uv_buf_init(data, suggested_size);
}


void UDPWrap::OnRecv(uv_udp_t* handle,
                     ssize_t nread,
                     uv_buf_t buf,
                     struct sockaddr* addr,
                     unsigned flags) {
  if (nread == 0) {
    if (buf.base != NULL)
      free(buf.base);
    return;
  }

  UDPWrap* wrap = reinterpret_cast<UDPWrap*>(handle->data);

  HandleScope scope(node_isolate);
  Local<Object> wrap_obj = wrap->object();
  Local<Value> argv[] = {
    Integer::New(nread, node_isolate),
    wrap_obj,
    Undefined(),
    Undefined()
  };

  if (nread < 0) {
    if (buf.base != NULL)
      free(buf.base);
    MakeCallback(wrap_obj, onmessage_sym, ARRAY_SIZE(argv), argv);
    return;
  }

  buf.base = static_cast<char*>(realloc(buf.base, nread));

  argv[2] = Buffer::Use(buf.base, nread);
  argv[3] = AddressToJS(addr);
  MakeCallback(wrap_obj, onmessage_sym, ARRAY_SIZE(argv), argv);
}


UDPWrap* UDPWrap::Unwrap(Local<Object> obj) {
  assert(!obj.IsEmpty());
  assert(obj->InternalFieldCount() > 0);
  return static_cast<UDPWrap*>(obj->GetAlignedPointerFromInternalField(0));
}


Local<Object> UDPWrap::Instantiate() {
  // If this assert fires then Initialize hasn't been called yet.
  assert(constructor.IsEmpty() == false);
  return NewInstance(constructor);
}


uv_udp_t* UDPWrap::UVHandle() {
  return &handle_;
}


}  // namespace node

NODE_MODULE(node_udp_wrap, node::UDPWrap::Initialize)
