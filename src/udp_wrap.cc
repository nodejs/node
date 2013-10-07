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
#include "env.h"
#include "env-inl.h"
#include "node_buffer.h"
#include "handle_wrap.h"
#include "req_wrap.h"
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
using v8::PropertyCallbackInfo;
using v8::String;
using v8::Uint32;
using v8::Undefined;
using v8::Value;


class SendWrap : public ReqWrap<uv_udp_send_t> {
 public:
  SendWrap(Environment* env, Local<Object> req_wrap_obj, bool have_callback);
  inline bool have_callback() const;
 private:
  const bool have_callback_;
};


SendWrap::SendWrap(Environment* env,
                   Local<Object> req_wrap_obj,
                   bool have_callback)
    : ReqWrap<uv_udp_send_t>(env, req_wrap_obj),
      have_callback_(have_callback) {
}


inline bool SendWrap::have_callback() const {
  return have_callback_;
}


UDPWrap::UDPWrap(Environment* env, Handle<Object> object)
    : HandleWrap(env, object, reinterpret_cast<uv_handle_t*>(&handle_)) {
  int r = uv_udp_init(env->event_loop(), &handle_);
  assert(r == 0);  // can't fail anyway
}


UDPWrap::~UDPWrap() {
}


void UDPWrap::Initialize(Handle<Object> target,
                         Handle<Value> unused,
                         Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(FIXED_ONE_BYTE_STRING(node_isolate, "UDP"));

  enum PropertyAttribute attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  t->InstanceTemplate()->SetAccessor(FIXED_ONE_BYTE_STRING(node_isolate, "fd"),
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


  AsyncWrap::AddMethods<UDPWrap>(t);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "UDP"), t->GetFunction());
  env->set_udp_constructor_function(t->GetFunction());
}


void UDPWrap::New(const FunctionCallbackInfo<Value>& args) {
  assert(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());
  new UDPWrap(env, args.This());
}


void UDPWrap::GetFD(Local<String>, const PropertyCallbackInfo<Value>& args) {
#if !defined(_WIN32)
  HandleScope scope(node_isolate);
  UDPWrap* wrap = Unwrap<UDPWrap>(args.This());
  int fd = (wrap == NULL) ? -1 : wrap->handle_.io_watcher.fd;
  args.GetReturnValue().Set(fd);
#endif
}


void UDPWrap::DoBind(const FunctionCallbackInfo<Value>& args, int family) {
  HandleScope scope(node_isolate);

  UDPWrap* wrap = Unwrap<UDPWrap>(args.This());

  // bind(ip, port, flags)
  assert(args.Length() == 3);

  String::Utf8Value address(args[0]);
  const int port = args[1]->Uint32Value();
  const int flags = args[2]->Uint32Value();
  char addr[sizeof(sockaddr_in6)];
  int err;

  switch (family) {
  case AF_INET:
    err = uv_ip4_addr(*address, port, reinterpret_cast<sockaddr_in*>(&addr));
    break;
  case AF_INET6:
    err = uv_ip6_addr(*address, port, reinterpret_cast<sockaddr_in6*>(&addr));
    break;
  default:
    assert(0 && "unexpected address family");
    abort();
  }

  if (err == 0) {
    err = uv_udp_bind(&wrap->handle_,
                      reinterpret_cast<const sockaddr*>(&addr),
                      flags);
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
    UDPWrap* wrap = Unwrap<UDPWrap>(args.This());                             \
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
  UDPWrap* wrap = Unwrap<UDPWrap>(args.This());

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
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  UDPWrap* wrap = Unwrap<UDPWrap>(args.This());

  // send(req, buffer, offset, length, port, address)
  assert(args[0]->IsObject());
  assert(Buffer::HasInstance(args[1]));
  assert(args[2]->IsUint32());
  assert(args[3]->IsUint32());
  assert(args[4]->IsUint32());
  assert(args[5]->IsString());
  assert(args[6]->IsBoolean());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  Local<Object> buffer_obj = args[1].As<Object>();
  size_t offset = args[2]->Uint32Value();
  size_t length = args[3]->Uint32Value();
  const unsigned short port = args[4]->Uint32Value();
  String::Utf8Value address(args[5]);
  const bool have_callback = args[6]->IsTrue();

  assert(offset < Buffer::Length(buffer_obj));
  assert(length <= Buffer::Length(buffer_obj) - offset);

  SendWrap* req_wrap = new SendWrap(env, req_wrap_obj, have_callback);

  uv_buf_t buf = uv_buf_init(Buffer::Data(buffer_obj) + offset,
                             length);
  char addr[sizeof(sockaddr_in6)];
  int err;

  switch (family) {
  case AF_INET:
    err = uv_ip4_addr(*address, port, reinterpret_cast<sockaddr_in*>(&addr));
    break;
  case AF_INET6:
    err = uv_ip6_addr(*address, port, reinterpret_cast<sockaddr_in6*>(&addr));
    break;
  default:
    assert(0 && "unexpected address family");
    abort();
  }

  if (err == 0) {
    err = uv_udp_send(&req_wrap->req_,
                      &wrap->handle_,
                      &buf,
                      1,
                      reinterpret_cast<const sockaddr*>(&addr),
                      OnSend);
  }

  req_wrap->Dispatched();
  if (err)
    delete req_wrap;

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
  UDPWrap* wrap = Unwrap<UDPWrap>(args.This());

  int err = uv_udp_recv_start(&wrap->handle_, OnAlloc, OnRecv);
  // UV_EALREADY means that the socket is already bound but that's okay
  if (err == UV_EALREADY)
    err = 0;
  args.GetReturnValue().Set(err);
}


void UDPWrap::RecvStop(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  UDPWrap* wrap = Unwrap<UDPWrap>(args.This());

  int r = uv_udp_recv_stop(&wrap->handle_);
  args.GetReturnValue().Set(r);
}


void UDPWrap::GetSockName(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  struct sockaddr_storage address;
  UDPWrap* wrap = Unwrap<UDPWrap>(args.This());

  assert(args[0]->IsObject());
  Local<Object> obj = args[0].As<Object>();

  int addrlen = sizeof(address);
  int err = uv_udp_getsockname(&wrap->handle_,
                               reinterpret_cast<sockaddr*>(&address),
                               &addrlen);

  if (err == 0) {
    const sockaddr* addr = reinterpret_cast<const sockaddr*>(&address);
    AddressToJS(env, addr, obj);
  }

  args.GetReturnValue().Set(err);
}


// TODO(bnoordhuis) share with StreamWrap::AfterWrite() in stream_wrap.cc
void UDPWrap::OnSend(uv_udp_send_t* req, int status) {
  SendWrap* req_wrap = static_cast<SendWrap*>(req->data);
  if (req_wrap->have_callback()) {
    Environment* env = req_wrap->env();
    Context::Scope context_scope(env->context());
    HandleScope handle_scope(env->isolate());
    Local<Value> arg = Integer::New(status, node_isolate);
    req_wrap->MakeCallback(env->oncomplete_string(), 1, &arg);
  }
  delete req_wrap;
}


void UDPWrap::OnAlloc(uv_handle_t* handle,
                      size_t suggested_size,
                      uv_buf_t* buf) {
  buf->base = static_cast<char*>(malloc(suggested_size));
  buf->len = suggested_size;

  if (buf->base == NULL && suggested_size > 0) {
    FatalError("node::UDPWrap::OnAlloc(uv_handle_t*, size_t, uv_buf_t*)",
               "Out Of Memory");
  }
}


void UDPWrap::OnRecv(uv_udp_t* handle,
                     ssize_t nread,
                     const uv_buf_t* buf,
                     const struct sockaddr* addr,
                     unsigned int flags) {
  if (nread == 0) {
    if (buf->base != NULL)
      free(buf->base);
    return;
  }

  UDPWrap* wrap = static_cast<UDPWrap*>(handle->data);
  Environment* env = wrap->env();

  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());

  Local<Object> wrap_obj = wrap->object();
  Local<Value> argv[] = {
    Integer::New(nread, node_isolate),
    wrap_obj,
    Undefined(env->isolate()),
    Undefined(env->isolate())
  };

  if (nread < 0) {
    if (buf->base != NULL)
      free(buf->base);
    wrap->MakeCallback(env->onmessage_string(), ARRAY_SIZE(argv), argv);
    return;
  }

  char* base = static_cast<char*>(realloc(buf->base, nread));
  argv[2] = Buffer::Use(env, base, nread);
  argv[3] = AddressToJS(env, addr);
  wrap->MakeCallback(env->onmessage_string(), ARRAY_SIZE(argv), argv);
}


Local<Object> UDPWrap::Instantiate(Environment* env) {
  // If this assert fires then Initialize hasn't been called yet.
  assert(env->udp_constructor_function().IsEmpty() == false);
  return env->udp_constructor_function()->NewInstance();
}


uv_udp_t* UDPWrap::UVHandle() {
  return &handle_;
}


}  // namespace node

NODE_MODULE_CONTEXT_AWARE(node_udp_wrap, node::UDPWrap::Initialize)
