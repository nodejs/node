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
#include <handle_wrap.h>

#include <stdlib.h>

using namespace v8;

namespace node {

#define UNWRAP                                                              \
  assert(!args.Holder().IsEmpty());                                         \
  assert(args.Holder()->InternalFieldCount() > 0);                          \
  FSEventWrap* wrap =                                                       \
      static_cast<FSEventWrap*>(args.Holder()->GetPointerFromInternalField(0)); \
  if (!wrap) {                                                              \
    uv_err_t err;                                                           \
    err.code = UV_EBADF;                                                    \
    SetErrno(err);                                                          \
    return scope.Close(Integer::New(-1));                                   \
  }

class FSEventWrap: public HandleWrap {
public:
  static void Initialize(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Start(const Arguments& args);
  static Handle<Value> Close(const Arguments& args);

private:
  FSEventWrap(Handle<Object> object);
  virtual ~FSEventWrap();

  static void OnEvent(uv_fs_event_t* handle, const char* filename, int events,
    int status);

  uv_fs_event_t handle_;
  bool initialized_;
};


FSEventWrap::FSEventWrap(Handle<Object> object): HandleWrap(object,
                                                    (uv_handle_t*)&handle_) {
  handle_.data = reinterpret_cast<void*>(this);
  initialized_ = false;
}


FSEventWrap::~FSEventWrap() {
  assert(initialized_ == false);
}


void FSEventWrap::Initialize(Handle<Object> target) {
  HandleWrap::Initialize(target);

  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("FSEvent"));

  NODE_SET_PROTOTYPE_METHOD(t, "start", Start);
  NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

  target->Set(String::NewSymbol("FSEvent"),
              Persistent<FunctionTemplate>::New(t)->GetFunction());
}


Handle<Value> FSEventWrap::New(const Arguments& args) {
  HandleScope scope;

  assert(args.IsConstructCall());
  new FSEventWrap(args.This());

  return scope.Close(args.This());
}


Handle<Value> FSEventWrap::Start(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  if (args.Length() < 1 || !args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New("Bad arguments")));
  }

  String::Utf8Value path(args[0]->ToString());

  int r = uv_fs_event_init(uv_default_loop(), &wrap->handle_, *path, OnEvent, 0);
  if (r == 0) {
    // Check for persistent argument
    if (!args[1]->IsTrue()) {
      uv_unref(uv_default_loop());
    }
    wrap->initialized_ = true;
  } else {
    SetErrno(uv_last_error(uv_default_loop()));
  }

  return scope.Close(Integer::New(r));
}


void FSEventWrap::OnEvent(uv_fs_event_t* handle, const char* filename,
    int events, int status) {
  HandleScope scope;
  Local<String> eventStr;

  FSEventWrap* wrap = reinterpret_cast<FSEventWrap*>(handle->data);

  assert(wrap->object_.IsEmpty() == false);

  // We're in a bind here. libuv can set both UV_RENAME and UV_CHANGE but
  // the Node API only lets us pass a single event to JS land.
  //
  // The obvious solution is to run the callback twice, once for each event.
  // However, since the second event is not allowed to fire if the handle is
  // closed after the first event, and since there is no good way to detect
  // closed handles, that option is out.
  //
  // For now, ignore the UV_CHANGE event if UV_RENAME is also set. Make the
  // assumption that a rename implicitly means an attribute change. Not too
  // unreasonable, right? Still, we should revisit this before v1.0.
  if (status) {
    SetErrno(uv_last_error(uv_default_loop()));
    eventStr = String::Empty();
  }
  else if (events & UV_RENAME) {
    eventStr = String::New("rename");
  }
  else if (events & UV_CHANGE) {
    eventStr = String::New("change");
  }
  else {
    assert(0 && "bad fs events flag");
    abort();
  }

  Local<Value> argv[3] = {
    Integer::New(status),
    eventStr,
    filename ? (Local<Value>)String::New(filename) : Local<Value>::New(v8::Null())
  };

  MakeCallback(wrap->object_, "onchange", 3, argv);
}


Handle<Value> FSEventWrap::Close(const Arguments& args) {
  HandleScope scope;

  UNWRAP

  if (!wrap->initialized_)
    return Undefined();

  wrap->initialized_ = false;
  return HandleWrap::Close(args);
}


} // namespace node

NODE_MODULE(node_fs_event_wrap, node::FSEventWrap::Initialize)
