#include "tcp_wrap.h"

#include "env.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "node_buffer.h"
#include "node_wrap.h"
#include "req-wrap.h"
#include "req-wrap-inl.h"
#include "stream_wrap.h"
#include "util.h"
#include "util-inl.h"

#include <stdlib.h>


namespace node {

using v8::Boolean;
using v8::Context;
using v8::EscapableHandleScope;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::PropertyAttribute;
using v8::String;
using v8::Undefined;
using v8::Value;


class TCPConnectWrap : public ReqWrap<uv_connect_t> {
 public:
  TCPConnectWrap(Environment* env, Local<Object> req_wrap_obj);
  size_t self_size() const override { return sizeof(*this); }
};


TCPConnectWrap::TCPConnectWrap(Environment* env, Local<Object> req_wrap_obj)
    : ReqWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_TCPCONNECTWRAP) {
  Wrap(req_wrap_obj, this);
}


static void NewTCPConnectWrap(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
}


Local<Object> TCPWrap::Instantiate(Environment* env, AsyncWrap* parent) {
  EscapableHandleScope handle_scope(env->isolate());
  CHECK_EQ(env->tcp_constructor_template().IsEmpty(), false);
  Local<Function> constructor = env->tcp_constructor_template()->GetFunction();
  CHECK_EQ(constructor.IsEmpty(), false);
  Local<Value> ptr = External::New(env->isolate(), parent);
  Local<Object> instance = constructor->NewInstance(1, &ptr);
  CHECK_EQ(instance.IsEmpty(), false);
  return handle_scope.Escape(instance);
}


void TCPWrap::Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);
  t->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "TCP"));
  t->InstanceTemplate()->SetInternalFieldCount(1);

  // Init properties
  t->InstanceTemplate()->Set(String::NewFromUtf8(env->isolate(), "reading"),
                             Boolean::New(env->isolate(), false));
  t->InstanceTemplate()->Set(String::NewFromUtf8(env->isolate(), "owner"),
                             Null(env->isolate()));
  t->InstanceTemplate()->Set(String::NewFromUtf8(env->isolate(), "onread"),
                             Null(env->isolate()));
  t->InstanceTemplate()->Set(String::NewFromUtf8(env->isolate(),
                                                 "onconnection"),
                             Null(env->isolate()));


  env->SetProtoMethod(t, "close", HandleWrap::Close);

  env->SetProtoMethod(t, "ref", HandleWrap::Ref);
  env->SetProtoMethod(t, "unref", HandleWrap::Unref);

  StreamWrap::AddMethods(env, t, StreamBase::kFlagHasWritev);

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

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "TCP"), t->GetFunction());
  env->set_tcp_constructor_template(t);

  // Create FunctionTemplate for TCPConnectWrap.
  Local<FunctionTemplate> cwt =
      FunctionTemplate::New(env->isolate(), NewTCPConnectWrap);
  cwt->InstanceTemplate()->SetInternalFieldCount(1);
  cwt->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "TCPConnectWrap"));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "TCPConnectWrap"),
              cwt->GetFunction());
}


uv_tcp_t* TCPWrap::UVHandle() {
  return &handle_;
}


void TCPWrap::New(const FunctionCallbackInfo<Value>& args) {
  // This constructor should not be exposed to public javascript.
  // Therefore we assert that we are not trying to call this as a
  // normal function.
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  TCPWrap* wrap;
  if (args.Length() == 0) {
    wrap = new TCPWrap(env, args.This(), nullptr);
  } else if (args[0]->IsExternal()) {
    void* ptr = args[0].As<External>()->Value();
    wrap = new TCPWrap(env, args.This(), static_cast<AsyncWrap*>(ptr));
  } else {
    UNREACHABLE();
  }
  CHECK(wrap);
}


TCPWrap::TCPWrap(Environment* env, Local<Object> object, AsyncWrap* parent)
    : StreamWrap(env,
                 object,
                 reinterpret_cast<uv_stream_t*>(&handle_),
                 AsyncWrap::PROVIDER_TCPWRAP,
                 parent) {
  int r = uv_tcp_init(env->event_loop(), &handle_);
  CHECK_EQ(r, 0);  // How do we proxy this error up to javascript?
                   // Suggestion: uv_tcp_init() returns void.
  UpdateWriteQueueSize();
}


TCPWrap::~TCPWrap() {
  CHECK(persistent().IsEmpty());
}


void TCPWrap::SetNoDelay(const FunctionCallbackInfo<Value>& args) {
  TCPWrap* wrap = Unwrap<TCPWrap>(args.Holder());
  int enable = static_cast<int>(args[0]->BooleanValue());
  int err = uv_tcp_nodelay(&wrap->handle_, enable);
  args.GetReturnValue().Set(err);
}


void TCPWrap::SetKeepAlive(const FunctionCallbackInfo<Value>& args) {
  TCPWrap* wrap = Unwrap<TCPWrap>(args.Holder());
  int enable = args[0]->Int32Value();
  unsigned int delay = args[1]->Uint32Value();
  int err = uv_tcp_keepalive(&wrap->handle_, enable, delay);
  args.GetReturnValue().Set(err);
}


#ifdef _WIN32
void TCPWrap::SetSimultaneousAccepts(const FunctionCallbackInfo<Value>& args) {
  TCPWrap* wrap = Unwrap<TCPWrap>(args.Holder());
  bool enable = args[0]->BooleanValue();
  int err = uv_tcp_simultaneous_accepts(&wrap->handle_, enable);
  args.GetReturnValue().Set(err);
}
#endif


void TCPWrap::Open(const FunctionCallbackInfo<Value>& args) {
  TCPWrap* wrap = Unwrap<TCPWrap>(args.Holder());
  int fd = static_cast<int>(args[0]->IntegerValue());
  uv_tcp_open(&wrap->handle_, fd);
}


void TCPWrap::Bind(const FunctionCallbackInfo<Value>& args) {
  TCPWrap* wrap = Unwrap<TCPWrap>(args.Holder());
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
  TCPWrap* wrap = Unwrap<TCPWrap>(args.Holder());
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
  TCPWrap* wrap = Unwrap<TCPWrap>(args.Holder());
  int backlog = args[0]->Int32Value();
  int err = uv_listen(reinterpret_cast<uv_stream_t*>(&wrap->handle_),
                      backlog,
                      OnConnection);
  args.GetReturnValue().Set(err);
}


void TCPWrap::OnConnection(uv_stream_t* handle, int status) {
  TCPWrap* tcp_wrap = static_cast<TCPWrap*>(handle->data);
  CHECK_EQ(&tcp_wrap->handle_, reinterpret_cast<uv_tcp_t*>(handle));
  Environment* env = tcp_wrap->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  // We should not be getting this callback if someone as already called
  // uv_close() on the handle.
  CHECK_EQ(tcp_wrap->persistent().IsEmpty(), false);

  Local<Value> argv[2] = {
    Integer::New(env->isolate(), status),
    Undefined(env->isolate())
  };

  if (status == 0) {
    // Instantiate the client javascript object and handle.
    Local<Object> client_obj =
        Instantiate(env, static_cast<AsyncWrap*>(tcp_wrap));

    // Unwrap the client javascript object.
    TCPWrap* wrap = Unwrap<TCPWrap>(client_obj);
    uv_stream_t* client_handle = reinterpret_cast<uv_stream_t*>(&wrap->handle_);
    if (uv_accept(handle, client_handle))
      return;

    // Successful accept. Call the onconnection callback in JavaScript land.
    argv[1] = client_obj;
  }

  tcp_wrap->MakeCallback(env->onconnection_string(), ARRAY_SIZE(argv), argv);
}


void TCPWrap::AfterConnect(uv_connect_t* req, int status) {
  TCPConnectWrap* req_wrap = static_cast<TCPConnectWrap*>(req->data);
  TCPWrap* wrap = static_cast<TCPWrap*>(req->handle->data);
  CHECK_EQ(req_wrap->env(), wrap->env());
  Environment* env = wrap->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  // The wrap and request objects should still be there.
  CHECK_EQ(req_wrap->persistent().IsEmpty(), false);
  CHECK_EQ(wrap->persistent().IsEmpty(), false);

  Local<Object> req_wrap_obj = req_wrap->object();
  Local<Value> argv[5] = {
    Integer::New(env->isolate(), status),
    wrap->object(),
    req_wrap_obj,
    v8::True(env->isolate()),
    v8::True(env->isolate())
  };

  req_wrap->MakeCallback(env->oncomplete_string(), ARRAY_SIZE(argv), argv);

  delete req_wrap;
}


void TCPWrap::Connect(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  TCPWrap* wrap = Unwrap<TCPWrap>(args.Holder());

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsUint32());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  node::Utf8Value ip_address(env->isolate(), args[1]);
  int port = args[2]->Uint32Value();

  sockaddr_in addr;
  int err = uv_ip4_addr(*ip_address, port, &addr);

  if (err == 0) {
    TCPConnectWrap* req_wrap = new TCPConnectWrap(env, req_wrap_obj);
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
  Environment* env = Environment::GetCurrent(args);

  TCPWrap* wrap = Unwrap<TCPWrap>(args.Holder());

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsUint32());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  node::Utf8Value ip_address(env->isolate(), args[1]);
  int port = args[2]->Int32Value();

  sockaddr_in6 addr;
  int err = uv_ip6_addr(*ip_address, port, &addr);

  if (err == 0) {
    TCPConnectWrap* req_wrap = new TCPConnectWrap(env, req_wrap_obj);
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

NODE_MODULE_CONTEXT_AWARE_BUILTIN(tcp_wrap, node::TCPWrap::Initialize)
