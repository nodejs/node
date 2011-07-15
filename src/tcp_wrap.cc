#include <node.h>
#include <node_buffer.h>

// Temporary hack: libuv should provide uv_inet_pton and uv_inet_ntop.
#ifdef __MINGW32__
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

#define SLAB_SIZE (1024 * 1024)
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Rules:
//
// - Do not throw from handle methods. Set errno.
//
// - MakeCallback may only be made directly off the event loop.
//   That is there can be no JavaScript stack frames underneith it.
//   (Is there anyway to assert that?)
//
// - No use of v8::WeakReferenceCallback. The close callback signifies that
//   we're done with a handle - external resources can be freed.
//
// - Reusable?
//
// - The uv_close_cb is used to free the c++ object. The close callback
//   is not made into javascript land.
//
// - uv_ref, uv_unref counts are managed at this layer to avoid needless
//   js/c++ boundary crossing. At the javascript layer that should all be
//   taken care of.


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

static Persistent<Function> constructor;
static size_t slab_used;
static uv_tcp_t* handle_that_last_alloced;

static Persistent<String> slab_sym;
static Persistent<String> buffer_sym;
static Persistent<String> family_symbol;
static Persistent<String> address_symbol;
static Persistent<String> port_symbol;
static Persistent<String> write_queue_size_sym;

class TCPWrap;

template <typename T>
class ReqWrap {
 public:
  ReqWrap() {
    HandleScope scope;
    object_ = Persistent<Object>::New(Object::New());
  }

  ~ReqWrap() {
    // Assert that someone has called Dispatched()
    assert(req_.data == this);
    assert(!object_.IsEmpty());
    object_.Dispose();
    object_.Clear();
  }

  // Call this after the req has been dispatched.
  void Dispatched() {
    req_.data = this;
  }

  Persistent<Object> object_;
  T req_;
};


typedef class ReqWrap<uv_shutdown_t> ShutdownWrap;
typedef class ReqWrap<uv_write_t> WriteWrap;
typedef class ReqWrap<uv_connect_t> ConnectWrap;


class TCPWrap {
 public:

  static void Initialize(Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->SetClassName(String::NewSymbol("TCP"));

    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "bind", Bind);
    NODE_SET_PROTOTYPE_METHOD(t, "listen", Listen);
    NODE_SET_PROTOTYPE_METHOD(t, "readStart", ReadStart);
    NODE_SET_PROTOTYPE_METHOD(t, "readStop", ReadStop);
    NODE_SET_PROTOTYPE_METHOD(t, "write", Write);
    NODE_SET_PROTOTYPE_METHOD(t, "connect", Connect);
    NODE_SET_PROTOTYPE_METHOD(t, "shutdown", Shutdown);
    NODE_SET_PROTOTYPE_METHOD(t, "close", Close);
    NODE_SET_PROTOTYPE_METHOD(t, "bind6", Bind6);
    NODE_SET_PROTOTYPE_METHOD(t, "connect6", Connect6);
    NODE_SET_PROTOTYPE_METHOD(t, "getsockname", GetSockName);

    constructor = Persistent<Function>::New(t->GetFunction());

    slab_sym = Persistent<String>::New(String::NewSymbol("slab"));
    buffer_sym = Persistent<String>::New(String::NewSymbol("buffer"));
    write_queue_size_sym =
      Persistent<String>::New(String::NewSymbol("writeQueueSize"));

    family_symbol = NODE_PSYMBOL("family");
    address_symbol = NODE_PSYMBOL("address");
    port_symbol = NODE_PSYMBOL("port");

    target->Set(String::NewSymbol("TCP"), constructor);
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

  TCPWrap(Handle<Object> object) {
    int r = uv_tcp_init(&handle_);
    handle_.data = this;
    assert(r == 0); // How do we proxy this error up to javascript?
                    // Suggestion: uv_tcp_init() returns void.
    assert(object_.IsEmpty());
    assert(object->InternalFieldCount() > 0);
    object_ = v8::Persistent<v8::Object>::New(object);
    object_->SetPointerInInternalField(0, this);

    UpdateWriteQueueSize();
  }

  ~TCPWrap() {
    assert(object_.IsEmpty());
  }

  // Free the C++ object on the close callback.
  static void OnClose(uv_handle_t* handle) {
    TCPWrap* wrap = static_cast<TCPWrap*>(handle->data);

    // The wrap object should still be there.
    assert(wrap->object_.IsEmpty() == false);

    wrap->object_->SetPointerInInternalField(0, NULL);
    wrap->object_.Dispose();
    wrap->object_.Clear();
    delete wrap;
  }

  inline void UpdateWriteQueueSize() {
    object_->Set(write_queue_size_sym, Integer::New(handle_.write_queue_size));
  }

  static Handle<Value> GetSockName(const Arguments& args) {
    HandleScope scope;
    struct sockaddr address;
    int family;
    int port;
    char ip[INET6_ADDRSTRLEN];

    UNWRAP

    int namelen = sizeof(address);
    int r = uv_getsockname(&wrap->handle_, &address, &namelen);

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

    int r = uv_tcp_listen(&wrap->handle_, backlog, OnConnection);

    // Error starting the tcp.
    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  static void OnConnection(uv_handle_t* handle, int status) {
    HandleScope scope;

    TCPWrap* wrap = static_cast<TCPWrap*>(handle->data);
    assert(&wrap->handle_ == (uv_tcp_t*)handle);

    // We should not be getting this callback if someone as already called
    // uv_close() on the handle.
    assert(wrap->object_.IsEmpty() == false);

    if (status != 0) {
      // TODO Handle server error (set errno and call onconnection with NULL)
      assert(0);
      return;
    }

    // Instanciate the client javascript object and handle.
    Local<Object> client_obj = constructor->NewInstance();

    // Unwrap the client javascript object.
    assert(client_obj->InternalFieldCount() > 0);
    TCPWrap* client_wrap =
        static_cast<TCPWrap*>(client_obj->GetPointerFromInternalField(0));

    int r = uv_accept(handle, (uv_stream_t*)&client_wrap->handle_);

    // uv_accept should always work.
    assert(r == 0);

    // Successful accept. Call the onconnection callback in JavaScript land.
    Local<Value> argv[1] = { client_obj };
    MakeCallback(wrap->object_, "onconnection", 1, argv);
  }

  static Handle<Value> ReadStart(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_read_start((uv_stream_t*)&wrap->handle_, OnAlloc, OnRead);

    // Error starting the tcp.
    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> ReadStop(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_read_stop((uv_stream_t*)&wrap->handle_);

    // Error starting the tcp.
    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  static inline char* NewSlab(Handle<Object> global, Handle<Object> wrap_obj) {
    Buffer* b = Buffer::New(SLAB_SIZE);
    global->SetHiddenValue(slab_sym, b->handle_);
    assert(Buffer::Length(b) == SLAB_SIZE);
    slab_used = 0;
    wrap_obj->SetHiddenValue(slab_sym, b->handle_);
    return Buffer::Data(b);
  }

  static uv_buf_t OnAlloc(uv_stream_t* handle, size_t suggested_size) {
    HandleScope scope;

    TCPWrap* wrap = static_cast<TCPWrap*>(handle->data);
    assert(&wrap->handle_ == (uv_tcp_t*)handle);

    char* slab = NULL;

    Handle<Object> global = Context::GetCurrent()->Global();
    Local<Value> slab_v = global->GetHiddenValue(slab_sym);

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
        wrap->object_->SetHiddenValue(slab_sym, slab_obj);
      }
    }

    uv_buf_t buf;
    buf.base = slab + slab_used;
    buf.len = MIN(SLAB_SIZE - slab_used, suggested_size);

    wrap->slab_offset_ = slab_used;
    slab_used += buf.len;

    handle_that_last_alloced = (uv_tcp_t*)handle;

    return buf;
  }

  static void OnRead(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
    HandleScope scope;

    TCPWrap* wrap = static_cast<TCPWrap*>(handle->data);

    // We should not be getting this callback if someone as already called
    // uv_close() on the handle.
    assert(wrap->object_.IsEmpty() == false);

    // Remove the reference to the slab to avoid memory leaks;
    Local<Value> slab_v = wrap->object_->GetHiddenValue(slab_sym);
    wrap->object_->SetHiddenValue(slab_sym, v8::Null());

    if (nread < 0)  {
      // EOF or Error
      if (handle_that_last_alloced == (uv_tcp_t*)handle) {
        slab_used -= buf.len;
      }

      SetErrno(uv_last_error().code);
      MakeCallback(wrap->object_, "onread", 0, NULL);
      return;
    }

    assert(nread <= buf.len);

    if (handle_that_last_alloced == (uv_tcp_t*)handle) {
      slab_used -= (buf.len - nread);
    }

    if (nread > 0) {
      Local<Value> argv[3] = {
        slab_v,
        Integer::New(wrap->slab_offset_),
        Integer::New(nread)
      };
      MakeCallback(wrap->object_, "onread", 3, argv);
    }
  }

  // TODO: share me?
  static Handle<Value> Close(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    assert(!wrap->object_.IsEmpty());
    int r = uv_close((uv_handle_t*) &wrap->handle_, OnClose);

    if (r) {
      SetErrno(uv_last_error().code);

      wrap->object_->SetPointerInInternalField(0, NULL);
      wrap->object_.Dispose();
      wrap->object_.Clear();
    }
    return scope.Close(Integer::New(r));
  }

  static void AfterWrite(uv_write_t* req, int status) {
    WriteWrap* req_wrap = (WriteWrap*) req->data;
    TCPWrap* wrap = (TCPWrap*) req->handle->data;

    HandleScope scope;

    // The wrap and request objects should still be there.
    assert(req_wrap->object_.IsEmpty() == false);
    assert(wrap->object_.IsEmpty() == false);

    if (status) {
      SetErrno(uv_last_error().code);
    }

    wrap->UpdateWriteQueueSize();

    Local<Value> argv[4] = {
      Integer::New(status),
      Local<Value>::New(wrap->object_),
      Local<Value>::New(req_wrap->object_),
      req_wrap->object_->GetHiddenValue(buffer_sym),
    };

    MakeCallback(req_wrap->object_, "oncomplete", 4, argv);

    delete req_wrap;
  }

  static Handle<Value> Write(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    // The first argument is a buffer.
    assert(Buffer::HasInstance(args[0]));
    Local<Object> buffer_obj = args[0]->ToObject();

    size_t offset = 0;
    size_t length = Buffer::Length(buffer_obj);

    if (args.Length() > 1) {
      offset = args[1]->IntegerValue();
    }

    if (args.Length() > 2) {
      length = args[2]->IntegerValue();
    }

    WriteWrap* req_wrap = new WriteWrap();

    req_wrap->object_->SetHiddenValue(buffer_sym, buffer_obj);

    uv_buf_t buf;
    buf.base = Buffer::Data(buffer_obj) + offset;
    buf.len = length;

    int r = uv_write(&req_wrap->req_, (uv_stream_t*)&wrap->handle_, &buf, 1,
        AfterWrite);

    req_wrap->Dispatched();

    wrap->UpdateWriteQueueSize();

    if (r) {
      SetErrno(uv_last_error().code);
      delete req_wrap;
      return scope.Close(v8::Null());
    } else {
      return scope.Close(req_wrap->object_);
    }
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

  static void AfterShutdown(uv_shutdown_t* req, int status) {
    ReqWrap<uv_shutdown_t>* req_wrap = (ReqWrap<uv_shutdown_t>*) req->data;
    TCPWrap* wrap = (TCPWrap*) req->handle->data;

    // The wrap and request objects should still be there.
    assert(req_wrap->object_.IsEmpty() == false);
    assert(wrap->object_.IsEmpty() == false);

    HandleScope scope;

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

  static Handle<Value> Shutdown(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    ShutdownWrap* req_wrap = new ShutdownWrap();

    int r = uv_shutdown(&req_wrap->req_, (uv_stream_t*) &wrap->handle_,
        AfterShutdown);

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
  Persistent<Object> object_;
  size_t slab_offset_;
};


}  // namespace node

NODE_MODULE(node_tcp_wrap, node::TCPWrap::Initialize);
