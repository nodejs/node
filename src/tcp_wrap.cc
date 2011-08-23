#include <node.h>
#include <node_buffer.h>
#include <req_wrap.h>
#include <handle_wrap.h>
#include <stream_wrap.h>

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
    SetErrno(UV_EBADF); \
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

Persistent<Function> tcpConstructor;

static Persistent<String> family_symbol;
static Persistent<String> address_symbol;
static Persistent<String> port_symbol;


typedef class ReqWrap<uv_connect_t> ConnectWrap;


class TCPWrap : public StreamWrap {
 public:

  static void Initialize(Handle<Object> target) {
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

    tcpConstructor = Persistent<Function>::New(t->GetFunction());

    family_symbol = NODE_PSYMBOL("family");
    address_symbol = NODE_PSYMBOL("address");
    port_symbol = NODE_PSYMBOL("port");

    target->Set(String::NewSymbol("TCP"), tcpConstructor);
  }

 private:
  static Handle<Value> New(const Arguments& args) {
    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());

    HandleScope scope;
    TCPWrap* wrap = new TCPWrap(args.This());
    assert(wrap);

    return scope.Close(args.This());
  }

  TCPWrap(Handle<Object> object) : StreamWrap(object,
                                              (uv_stream_t*) &handle_) {
    int r = uv_tcp_init(&handle_);
    assert(r == 0); // How do we proxy this error up to javascript?
                    // Suggestion: uv_tcp_init() returns void.
    UpdateWriteQueueSize();
  }

  ~TCPWrap() {
    assert(object_.IsEmpty());
  }

  static Handle<Value> GetSockName(const Arguments& args) {
    HandleScope scope;
    struct sockaddr address;
    int family;
    int port;
    char ip[INET6_ADDRSTRLEN];

    UNWRAP

    int addrlen = sizeof(address);
    int r = uv_getsockname(reinterpret_cast<uv_handle_t*>(&wrap->handle_),
                           reinterpret_cast<sockaddr*>(&address),
                           &addrlen);

    Local<Object> sockname = Object::New();
    if (r != 0) {
      SetErrno(uv_last_error().code);
    } else {
      family = address.sa_family;
      if (family == AF_INET) {
        struct sockaddr_in* addrin = (struct sockaddr_in*)&address;
        uv_inet_ntop(AF_INET, &(addrin->sin_addr), ip, INET6_ADDRSTRLEN);
        port = ntohs(addrin->sin_port);
      } else if (family == AF_INET6) {
        struct sockaddr_in6* addrin6 = (struct sockaddr_in6*)&address;
        uv_inet_ntop(AF_INET6, &(addrin6->sin6_addr), ip, INET6_ADDRSTRLEN);
        port = ntohs(addrin6->sin6_port);
      }

      sockname->Set(port_symbol, Integer::New(port));
      sockname->Set(family_symbol, Integer::New(family));
      sockname->Set(address_symbol, String::New(ip));
    }

    return scope.Close(sockname);

  }


  static Handle<Value> Bind(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    String::AsciiValue ip_address(args[0]->ToString());
    int port = args[1]->Int32Value();

    struct sockaddr_in address = uv_ip4_addr(*ip_address, port);
    int r = uv_tcp_bind(&wrap->handle_, address);

    // Error starting the tcp.
    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Bind6(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    String::AsciiValue ip6_address(args[0]->ToString());
    int port = args[1]->Int32Value();

    struct sockaddr_in6 address = uv_ip6_addr(*ip6_address, port);
    int r = uv_tcp_bind6(&wrap->handle_, address);

    // Error starting the tcp.
    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Listen(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int backlog = args[0]->Int32Value();

    int r = uv_listen((uv_stream_t*)&wrap->handle_, backlog, OnConnection);

    // Error starting the tcp.
    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  static void OnConnection(uv_stream_t* handle, int status) {
    HandleScope scope;

    TCPWrap* wrap = static_cast<TCPWrap*>(handle->data);
    assert(&wrap->handle_ == (uv_tcp_t*)handle);

    // We should not be getting this callback if someone as already called
    // uv_close() on the handle.
    assert(wrap->object_.IsEmpty() == false);

    Handle<Value> argv[1];

    if (status == 0) {
      // Instantiate the client javascript object and handle.
      Local<Object> client_obj = tcpConstructor->NewInstance();

      // Unwrap the client javascript object.
      assert(client_obj->InternalFieldCount() > 0);
      TCPWrap* client_wrap =
          static_cast<TCPWrap*>(client_obj->GetPointerFromInternalField(0));

      int r = uv_accept(handle, (uv_stream_t*)&client_wrap->handle_);

      // uv_accept should always work.
      assert(r == 0);

      // Successful accept. Call the onconnection callback in JavaScript land.
      argv[0] = client_obj;
    } else {
      SetErrno(uv_last_error().code);
      argv[0] = v8::Null();
    }

    MakeCallback(wrap->object_, "onconnection", 1, argv);
  }

  static void AfterConnect(uv_connect_t* req, int status) {
    ConnectWrap* req_wrap = (ConnectWrap*) req->data;
    TCPWrap* wrap = (TCPWrap*) req->handle->data;

    HandleScope scope;

    // The wrap and request objects should still be there.
    assert(req_wrap->object_.IsEmpty() == false);
    assert(wrap->object_.IsEmpty() == false);

    if (status) {
      SetErrno(uv_last_error().code);
    }

    Local<Value> argv[3] = {
      Integer::New(status),
      Local<Value>::New(wrap->object_),
      Local<Value>::New(req_wrap->object_)
    };

    MakeCallback(req_wrap->object_, "oncomplete", 3, argv);

    delete req_wrap;
  }

  static Handle<Value> Connect(const Arguments& args) {
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
      SetErrno(uv_last_error().code);
      delete req_wrap;
      return scope.Close(v8::Null());
    } else {
      return scope.Close(req_wrap->object_);
    }
  }

  static Handle<Value> Connect6(const Arguments& args) {
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
      SetErrno(uv_last_error().code);
      delete req_wrap;
      return scope.Close(v8::Null());
    } else {
      return scope.Close(req_wrap->object_);
    }
  }


  uv_tcp_t handle_;
};


}  // namespace node

NODE_MODULE(node_tcp_wrap, node::TCPWrap::Initialize);
