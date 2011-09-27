#include <node.h>
#include <node_buffer.h>
#include <req_wrap.h>
#include <handle_wrap.h>
#include <stream_wrap.h>

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
using v8::Undefined;

#define UNWRAP \
  assert(!args.Holder().IsEmpty()); \
  assert(args.Holder()->InternalFieldCount() > 0); \
  TTYWrap* wrap =  \
      static_cast<TTYWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
  if (!wrap) { \
    SetErrno(UV_EBADF); \
    return scope.Close(Integer::New(-1)); \
  }


class TTYWrap : StreamWrap {
 public:
  static void Initialize(Handle<Object> target) {
    StreamWrap::Initialize(target);

    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->SetClassName(String::NewSymbol("TTY"));

    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "close", HandleWrap::Close);
    NODE_SET_PROTOTYPE_METHOD(t, "unref", HandleWrap::Unref);

    NODE_SET_PROTOTYPE_METHOD(t, "readStart", StreamWrap::ReadStart);
    NODE_SET_PROTOTYPE_METHOD(t, "readStop", StreamWrap::ReadStop);
    NODE_SET_PROTOTYPE_METHOD(t, "write", StreamWrap::Write);

    NODE_SET_PROTOTYPE_METHOD(t, "getWindowSize", TTYWrap::GetWindowSize);
    NODE_SET_PROTOTYPE_METHOD(t, "setRawMode", SetRawMode);

    NODE_SET_METHOD(target, "isTTY", IsTTY);
    NODE_SET_METHOD(target, "guessHandleType", GuessHandleType);

    target->Set(String::NewSymbol("TTY"), t->GetFunction());
  }

 private:
  static Handle<Value> GuessHandleType(const Arguments& args) {
    HandleScope scope;
    int fd = args[0]->Int32Value();
    assert(fd >= 0);

    uv_handle_type t = uv_guess_handle(fd);

    switch (t) {
      case UV_TTY:
        return scope.Close(String::New("TTY"));

      case UV_NAMED_PIPE:
        return scope.Close(String::New("PIPE"));

      case UV_FILE:
        return scope.Close(String::New("FILE"));

      default:
        assert(0);
        return v8::Undefined();
    }
  }

  static Handle<Value> IsTTY(const Arguments& args) {
    HandleScope scope;
    int fd = args[0]->Int32Value();
    assert(fd >= 0);
    return uv_guess_handle(fd) == UV_TTY ? v8::True() : v8::False();
  }

  static Handle<Value> GetWindowSize(const Arguments& args) {
    HandleScope scope;
    
    UNWRAP

    int width, height;
    int r = uv_tty_get_winsize(&wrap->handle_, &width, &height);

    if (r) {
      SetErrno(uv_last_error(uv_default_loop()).code);
      return v8::Undefined();
    }

    Local<v8::Array> a = v8::Array::New(2);
    a->Set(0, Integer::New(width));
    a->Set(1, Integer::New(height));

    return scope.Close(a);
  }

  static Handle<Value> SetRawMode(const Arguments& args) {
    HandleScope scope;

    UNWRAP

    int r = uv_tty_set_mode(&wrap->handle_, args[0]->IsTrue());

    if (r) {
      SetErrno(uv_last_error(uv_default_loop()).code);
    }

    return scope.Close(Integer::New(r));
  }

  static Handle<Value> New(const Arguments& args) {
    HandleScope scope;

    // This constructor should not be exposed to public javascript.
    // Therefore we assert that we are not trying to call this as a
    // normal function.
    assert(args.IsConstructCall());

    int fd = args[0]->Int32Value();
    assert(fd >= 0);

    TTYWrap* wrap = new TTYWrap(args.This(), fd);
    assert(wrap);
    wrap->UpdateWriteQueueSize();

    return scope.Close(args.This());
  }

  TTYWrap(Handle<Object> object, int fd)
      : StreamWrap(object, (uv_stream_t*)&handle_) {
    uv_tty_init(uv_default_loop(), &handle_, fd);
  }

  uv_tty_t handle_;
};

}  // namespace node

NODE_MODULE(node_tty_wrap, node::TTYWrap::Initialize);
