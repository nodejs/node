#include "udp_wrap.h"
#include "env.h"
#include "env-inl.h"
#include "node_buffer.h"
#include "handle_wrap.h"
#include "req-wrap.h"
#include "req-wrap-inl.h"
#include "util.h"
#include "util-inl.h"

#include <stdlib.h>


namespace node {

using v8::Array;
using v8::Context;
using v8::EscapableHandleScope;
using v8::External;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::PropertyAttribute;
using v8::PropertyCallbackInfo;
using v8::String;
using v8::Undefined;
using v8::Value;


class SendWrap : public ReqWrap<uv_udp_send_t> {
 public:
  SendWrap(Environment* env, Local<Object> req_wrap_obj, bool have_callback);
  inline bool have_callback() const;
  size_t msg_size;
  size_t self_size() const override { return sizeof(*this); }
 private:
  const bool have_callback_;
};


SendWrap::SendWrap(Environment* env,
                   Local<Object> req_wrap_obj,
                   bool have_callback)
    : ReqWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_UDPSENDWRAP),
      have_callback_(have_callback) {
  Wrap(req_wrap_obj, this);
}


inline bool SendWrap::have_callback() const {
  return have_callback_;
}


static void NewSendWrap(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
}


UDPWrap::UDPWrap(Environment* env, Local<Object> object, AsyncWrap* parent)
    : HandleWrap(env,
                 object,
                 reinterpret_cast<uv_handle_t*>(&handle_),
                 AsyncWrap::PROVIDER_UDPWRAP) {
  int r = uv_udp_init(env->event_loop(), &handle_);
  CHECK_EQ(r, 0);  // can't fail anyway
}


void UDPWrap::Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "UDP"));

  enum PropertyAttribute attributes =
      static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  t->InstanceTemplate()->SetAccessor(env->fd_string(),
                                     UDPWrap::GetFD,
                                     nullptr,
                                     env->as_external(),
                                     v8::DEFAULT,
                                     attributes);

  env->SetProtoMethod(t, "bind", Bind);
  env->SetProtoMethod(t, "send", Send);
  env->SetProtoMethod(t, "bind6", Bind6);
  env->SetProtoMethod(t, "send6", Send6);
  env->SetProtoMethod(t, "close", Close);
  env->SetProtoMethod(t, "recvStart", RecvStart);
  env->SetProtoMethod(t, "recvStop", RecvStop);
  env->SetProtoMethod(t, "getsockname",
                      GetSockOrPeerName<UDPWrap, uv_udp_getsockname>);
  env->SetProtoMethod(t, "addMembership", AddMembership);
  env->SetProtoMethod(t, "dropMembership", DropMembership);
  env->SetProtoMethod(t, "setMulticastTTL", SetMulticastTTL);
  env->SetProtoMethod(t, "setMulticastLoopback", SetMulticastLoopback);
  env->SetProtoMethod(t, "setBroadcast", SetBroadcast);
  env->SetProtoMethod(t, "setTTL", SetTTL);

  env->SetProtoMethod(t, "ref", HandleWrap::Ref);
  env->SetProtoMethod(t, "unref", HandleWrap::Unref);
  env->SetProtoMethod(t, "hasRef", HandleWrap::HasRef);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "UDP"), t->GetFunction());
  env->set_udp_constructor_function(t->GetFunction());

  // Create FunctionTemplate for SendWrap
  Local<FunctionTemplate> swt =
      FunctionTemplate::New(env->isolate(), NewSendWrap);
  swt->InstanceTemplate()->SetInternalFieldCount(1);
  swt->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "SendWrap"));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "SendWrap"),
              swt->GetFunction());
}


void UDPWrap::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() == 0) {
    new UDPWrap(env, args.This(), nullptr);
  } else if (args[0]->IsExternal()) {
    new UDPWrap(env,
                args.This(),
                static_cast<AsyncWrap*>(args[0].As<External>()->Value()));
  } else {
    UNREACHABLE();
  }
}


void UDPWrap::GetFD(Local<String>, const PropertyCallbackInfo<Value>& args) {
  int fd = UV_EBADF;
#if !defined(_WIN32)
  HandleScope scope(args.GetIsolate());
  UDPWrap* wrap = Unwrap<UDPWrap>(args.Holder());
  if (wrap != nullptr)
    uv_fileno(reinterpret_cast<uv_handle_t*>(&wrap->handle_), &fd);
#endif
  args.GetReturnValue().Set(fd);
}


void UDPWrap::DoBind(const FunctionCallbackInfo<Value>& args, int family) {
  UDPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));

  // bind(ip, port, flags)
  CHECK_EQ(args.Length(), 3);

  node::Utf8Value address(args.GetIsolate(), args[0]);
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
    CHECK(0 && "unexpected address family");
    ABORT();
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
    UDPWrap* wrap = Unwrap<UDPWrap>(args.Holder());                           \
    CHECK_EQ(args.Length(), 1);                                               \
    int flag = args[0]->Int32Value();                                         \
    int err = wrap == nullptr ? UV_EBADF : fn(&wrap->handle_, flag);          \
    args.GetReturnValue().Set(err);                                           \
  }

X(SetTTL, uv_udp_set_ttl)
X(SetBroadcast, uv_udp_set_broadcast)
X(SetMulticastTTL, uv_udp_set_multicast_ttl)
X(SetMulticastLoopback, uv_udp_set_multicast_loop)

#undef X


void UDPWrap::SetMembership(const FunctionCallbackInfo<Value>& args,
                            uv_membership membership) {
  UDPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));

  CHECK_EQ(args.Length(), 2);

  node::Utf8Value address(args.GetIsolate(), args[0]);
  node::Utf8Value iface(args.GetIsolate(), args[1]);

  const char* iface_cstr = *iface;
  if (args[1]->IsUndefined() || args[1]->IsNull()) {
      iface_cstr = nullptr;
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
  Environment* env = Environment::GetCurrent(args);

  UDPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));

  // send(req, list, list.length, port, address, hasCallback)
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsArray());
  CHECK(args[2]->IsUint32());
  CHECK(args[3]->IsUint32());
  CHECK(args[4]->IsString());
  CHECK(args[5]->IsBoolean());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  Local<Array> chunks = args[1].As<Array>();
  // it is faster to fetch the length of the
  // array in js-land
  size_t count = args[2]->Uint32Value();
  const unsigned short port = args[3]->Uint32Value();
  node::Utf8Value address(env->isolate(), args[4]);
  const bool have_callback = args[5]->IsTrue();

  SendWrap* req_wrap = new SendWrap(env, req_wrap_obj, have_callback);
  size_t msg_size = 0;

  MaybeStackBuffer<uv_buf_t, 16> bufs(count);

  // construct uv_buf_t array
  for (size_t i = 0; i < count; i++) {
    Local<Value> chunk = chunks->Get(i);

    size_t length = Buffer::Length(chunk);

    bufs[i] = uv_buf_init(Buffer::Data(chunk), length);
    msg_size += length;
  }

  req_wrap->msg_size = msg_size;

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
    CHECK(0 && "unexpected address family");
    ABORT();
  }

  if (err == 0) {
    err = uv_udp_send(req_wrap->req(),
                      &wrap->handle_,
                      *bufs,
                      count,
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
  UDPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  int err = uv_udp_recv_start(&wrap->handle_, OnAlloc, OnRecv);
  // UV_EALREADY means that the socket is already bound but that's okay
  if (err == UV_EALREADY)
    err = 0;
  args.GetReturnValue().Set(err);
}


void UDPWrap::RecvStop(const FunctionCallbackInfo<Value>& args) {
  UDPWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap,
                          args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  int r = uv_udp_recv_stop(&wrap->handle_);
  args.GetReturnValue().Set(r);
}


void UDPWrap::OnSend(uv_udp_send_t* req, int status) {
  SendWrap* req_wrap = static_cast<SendWrap*>(req->data);
  if (req_wrap->have_callback()) {
    Environment* env = req_wrap->env();
    HandleScope handle_scope(env->isolate());
    Context::Scope context_scope(env->context());
    Local<Value> arg[] = {
      Integer::New(env->isolate(), status),
      Integer::New(env->isolate(), req_wrap->msg_size),
    };
    req_wrap->MakeCallback(env->oncomplete_string(), 2, arg);
  }
  delete req_wrap;
}


void UDPWrap::OnAlloc(uv_handle_t* handle,
                      size_t suggested_size,
                      uv_buf_t* buf) {
  buf->base = node::Malloc(suggested_size);
  buf->len = suggested_size;
}


void UDPWrap::OnRecv(uv_udp_t* handle,
                     ssize_t nread,
                     const uv_buf_t* buf,
                     const struct sockaddr* addr,
                     unsigned int flags) {
  if (nread == 0 && addr == nullptr) {
    if (buf->base != nullptr)
      free(buf->base);
    return;
  }

  UDPWrap* wrap = static_cast<UDPWrap*>(handle->data);
  Environment* env = wrap->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Object> wrap_obj = wrap->object();
  Local<Value> argv[] = {
    Integer::New(env->isolate(), nread),
    wrap_obj,
    Undefined(env->isolate()),
    Undefined(env->isolate())
  };

  if (nread < 0) {
    if (buf->base != nullptr)
      free(buf->base);
    wrap->MakeCallback(env->onmessage_string(), arraysize(argv), argv);
    return;
  }

  char* base = node::UncheckedRealloc(buf->base, nread);
  argv[2] = Buffer::New(env, base, nread).ToLocalChecked();
  argv[3] = AddressToJS(env, addr);
  wrap->MakeCallback(env->onmessage_string(), arraysize(argv), argv);
}


Local<Object> UDPWrap::Instantiate(Environment* env, AsyncWrap* parent) {
  EscapableHandleScope scope(env->isolate());
  // If this assert fires then Initialize hasn't been called yet.
  CHECK_EQ(env->udp_constructor_function().IsEmpty(), false);
  Local<Value> ptr = External::New(env->isolate(), parent);
  return scope.Escape(env->udp_constructor_function()
      ->NewInstance(env->context(), 1, &ptr).ToLocalChecked());
}


uv_udp_t* UDPWrap::UVHandle() {
  return &handle_;
}


}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(udp_wrap, node::UDPWrap::Initialize)
