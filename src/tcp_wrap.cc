#include <v8.h>
#include <uv.h>
#include <node.h>
#include <node_buffer.h>

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
static Persistent<String> offset_sym;
static Persistent<String> length_sym;

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
    NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

    constructor = Persistent<Function>::New(t->GetFunction());

    slab_sym = Persistent<String>::New(String::NewSymbol("slab"));
    offset_sym = Persistent<String>::New(String::NewSymbol("offset"));
    length_sym = Persistent<String>::New(String::NewSymbol("length"));

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
  }

  ~TCPWrap() {
    assert(object_.IsEmpty());
  }

  // Free the C++ object on the close callback.
  static void OnClose(uv_handle_t* handle) {
    TCPWrap* wrap = static_cast<TCPWrap*>(handle->data);
    delete wrap;
  }

  static Handle<Value> Bind(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    String::AsciiValue ip_address(args[0]->ToString());
    int port = args[1]->Int32Value();

    struct sockaddr_in address = uv_ip4_addr(*ip_address, port);
    int r = uv_bind(&wrap->handle_, address);

    // Error starting the tcp.
    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> Listen(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int backlog = args[0]->Int32Value();

    int r = uv_listen(&wrap->handle_, backlog, OnConnection);

    // Error starting the tcp.
    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  static void OnConnection(uv_tcp_t* handle, int status) {
    HandleScope scope;

    TCPWrap* wrap = static_cast<TCPWrap*>(handle->data);

    assert(&wrap->handle_ == handle);

    if (status != 0) {
      // TODO Handle server error (call onerror?)
      assert(0);
      uv_close((uv_handle_t*) handle, OnClose);
      return;
    }

    // Instanciate the client javascript object and handle.
    Local<Object> client_obj = constructor->NewInstance();

    // Unwrap the client javascript object.
    assert(client_obj->InternalFieldCount() > 0);
    TCPWrap* client_wrap =
        static_cast<TCPWrap*>(client_obj->GetPointerFromInternalField(0));

    int r = uv_accept(handle, &client_wrap->handle_);

    // uv_accept should always work.
    assert(r == 0);

    // Successful accept. Call the onconnection callback in JavaScript land.
    Local<Value> argv[1] = { client_obj };
    MakeCallback(wrap->object_, "onconnection", 1, argv);
  }

  static Handle<Value> ReadStart(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_read_start(&wrap->handle_, OnAlloc, OnRead);

    // Error starting the tcp.
    if (r) SetErrno(uv_last_error().code);

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> ReadStop(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_read_stop(&wrap->handle_);

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

  static uv_buf_t OnAlloc(uv_tcp_t* handle, size_t suggested_size) {
    HandleScope scope;

    TCPWrap* wrap = static_cast<TCPWrap*>(handle->data);
    assert(&wrap->handle_ == handle);

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
      assert(SLAB_SIZE > slab_used);

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

    handle_that_last_alloced = handle;

    return buf;
  }

  static void OnRead(uv_tcp_t* handle, ssize_t nread, uv_buf_t buf) {
    HandleScope scope;

    TCPWrap* wrap = static_cast<TCPWrap*>(handle->data);

    // Remove the reference to the slab to avoid memory leaks;
    Local<Value> slab_v = wrap->object_->GetHiddenValue(slab_sym);
    wrap->object_->SetHiddenValue(slab_sym, v8::Null());

    if (nread < 0)  {
      // EOF or Error
      if (handle_that_last_alloced == handle) {
        slab_used -= buf.len;
      }

      SetErrno(uv_last_error().code);
      MakeCallback(wrap->object_, "onread", 0, NULL);
      return;
    }

    assert(nread <= buf.len);

    if (handle_that_last_alloced == handle) {
      slab_used -= (buf.len - nread);
    }

    if (nread > 0) {
      Local<Object> slice = Object::New();
      slice->Set(slab_sym, slab_v);
      slice->Set(offset_sym, Integer::New(wrap->slab_offset_));
      slice->Set(length_sym, Integer::New(nread));

      Local<Value> argv[1] = { slice };
      MakeCallback(wrap->object_, "onread", 1, argv);
    }
  }


  // TODO: share me?
  static Handle<Value> Close(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_close((uv_handle_t*) &wrap->handle_, OnClose);

    if (r) SetErrno(uv_last_error().code);

    assert(!wrap->object_.IsEmpty());
    wrap->object_->SetPointerInInternalField(0, NULL);
    wrap->object_.Dispose();
    wrap->object_.Clear();

    return scope.Close(Integer::New(r));
  }

  uv_tcp_t handle_;
  Persistent<Object> object_;
  size_t slab_offset_;
};


}  // namespace node

NODE_MODULE(node_tcp_wrap, node::TCPWrap::Initialize);
