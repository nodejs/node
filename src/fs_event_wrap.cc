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

#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "node.h"
#include "handle_wrap.h"

#include <stdlib.h>

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

class FSEventWrap: public HandleWrap {
 public:
  static void Initialize(Handle<Object> target,
                         Handle<Value> unused,
                         Handle<Context> context);
  static void New(const FunctionCallbackInfo<Value>& args);
  static void Start(const FunctionCallbackInfo<Value>& args);
  static void Close(const FunctionCallbackInfo<Value>& args);

 private:
  FSEventWrap(Environment* env, Handle<Object> object);
  virtual ~FSEventWrap();

  static void OnEvent(uv_fs_event_t* handle, const char* filename, int events,
    int status);

  uv_fs_event_t handle_;
  bool initialized_;
};


FSEventWrap::FSEventWrap(Environment* env, Handle<Object> object)
    : HandleWrap(env, object, reinterpret_cast<uv_handle_t*>(&handle_)) {
  initialized_ = false;
}


FSEventWrap::~FSEventWrap() {
  assert(initialized_ == false);
}


void FSEventWrap::Initialize(Handle<Object> target,
                             Handle<Value> unused,
                             Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  HandleScope handle_scope(env->isolate());

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(FIXED_ONE_BYTE_STRING(node_isolate, "FSEvent"));

  NODE_SET_PROTOTYPE_METHOD(t, "start", Start);
  NODE_SET_PROTOTYPE_METHOD(t, "close", Close);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "FSEvent"), t->GetFunction());
}


void FSEventWrap::New(const FunctionCallbackInfo<Value>& args) {
  assert(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  new FSEventWrap(env, args.This());
}


void FSEventWrap::Start(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  FSEventWrap* wrap = Unwrap<FSEventWrap>(args.This());

  if (args.Length() < 1 || !args[0]->IsString()) {
    return ThrowTypeError("Bad arguments");
  }

  String::Utf8Value path(args[0]);

  int err = uv_fs_event_init(wrap->env()->event_loop(), &wrap->handle_);

  if (err == 0) {
    wrap->initialized_ = true;

    err = uv_fs_event_start(&wrap->handle_, OnEvent, *path, 0);

    if (err == 0) {
      // Check for persistent argument
      if (!args[1]->IsTrue()) {
        uv_unref(reinterpret_cast<uv_handle_t*>(&wrap->handle_));
      }
    } else {
      FSEventWrap::Close(args);
    }
  }

  args.GetReturnValue().Set(err);
}


void FSEventWrap::OnEvent(uv_fs_event_t* handle, const char* filename,
    int events, int status) {
  FSEventWrap* wrap = static_cast<FSEventWrap*>(handle->data);
  Environment* env = wrap->env();

  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());

  assert(wrap->persistent().IsEmpty() == false);

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
  Local<String> event_string;
  if (status) {
    event_string = String::Empty(node_isolate);
  } else if (events & UV_RENAME) {
    event_string = env->rename_string();
  } else if (events & UV_CHANGE) {
    event_string = env->change_string();
  } else {
    assert(0 && "bad fs events flag");
    abort();
  }

  Local<Value> argv[] = {
    Integer::New(status, node_isolate),
    event_string,
    Null(node_isolate)
  };

  if (filename != NULL) {
    argv[2] = OneByteString(node_isolate, filename);
  }

  MakeCallback(env,
               wrap->object(),
               env->onchange_string(),
               ARRAY_SIZE(argv),
               argv);
}


void FSEventWrap::Close(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  FSEventWrap* wrap = Unwrap<FSEventWrap>(args.This());

  if (wrap == NULL || wrap->initialized_ == false)
    return;
  wrap->initialized_ = false;

  HandleWrap::Close(args);
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE(node_fs_event_wrap, node::FSEventWrap::Initialize)
