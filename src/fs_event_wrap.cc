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

#include "async-wrap-inl.h"
#include "env-inl.h"
#include "util-inl.h"
#include "node.h"
#include "handle_wrap.h"
#include "string_bytes.h"

#include <stdlib.h>

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Value;

namespace {

class FSEventWrap: public HandleWrap {
 public:
  static void Initialize(Local<Object> target,
                         Local<Value> unused,
                         Local<Context> context);
  static void New(const FunctionCallbackInfo<Value>& args);
  static void Start(const FunctionCallbackInfo<Value>& args);
  static void Close(const FunctionCallbackInfo<Value>& args);

  size_t self_size() const override { return sizeof(*this); }

 private:
  static const encoding kDefaultEncoding = UTF8;

  FSEventWrap(Environment* env, Local<Object> object);
  ~FSEventWrap() override;

  static void OnEvent(uv_fs_event_t* handle, const char* filename, int events,
    int status);

  uv_fs_event_t handle_;
  bool initialized_ = false;
  enum encoding encoding_ = kDefaultEncoding;
};


FSEventWrap::FSEventWrap(Environment* env, Local<Object> object)
    : HandleWrap(env,
                 object,
                 reinterpret_cast<uv_handle_t*>(&handle_),
                 AsyncWrap::PROVIDER_FSEVENTWRAP) {}


FSEventWrap::~FSEventWrap() {
  CHECK_EQ(initialized_, false);
}


void FSEventWrap::Initialize(Local<Object> target,
                             Local<Value> unused,
                             Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  auto fsevent_string = FIXED_ONE_BYTE_STRING(env->isolate(), "FSEvent");
  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(fsevent_string);

  AsyncWrap::AddWrapMethods(env, t);
  env->SetProtoMethod(t, "start", Start);
  env->SetProtoMethod(t, "close", Close);

  target->Set(fsevent_string, t->GetFunction());
}


void FSEventWrap::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new FSEventWrap(env, args.This());
}


void FSEventWrap::Start(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  FSEventWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  CHECK_EQ(wrap->initialized_, false);

  static const char kErrMsg[] = "filename must be a string or Buffer";
  if (args.Length() < 1)
    return env->ThrowTypeError(kErrMsg);

  BufferValue path(env->isolate(), args[0]);
  if (*path == nullptr)
    return env->ThrowTypeError(kErrMsg);

  unsigned int flags = 0;
  if (args[2]->IsTrue())
    flags |= UV_FS_EVENT_RECURSIVE;

  wrap->encoding_ = ParseEncoding(env->isolate(), args[3], kDefaultEncoding);

  int err = uv_fs_event_init(wrap->env()->event_loop(), &wrap->handle_);
  if (err == 0) {
    wrap->initialized_ = true;

    err = uv_fs_event_start(&wrap->handle_, OnEvent, *path, flags);

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

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  CHECK_EQ(wrap->persistent().IsEmpty(), false);

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
    event_string = String::Empty(env->isolate());
  } else if (events & UV_RENAME) {
    event_string = env->rename_string();
  } else if (events & UV_CHANGE) {
    event_string = env->change_string();
  } else {
    CHECK(0 && "bad fs events flag");
  }

  Local<Value> argv[] = {
    Integer::New(env->isolate(), status),
    event_string,
    Null(env->isolate())
  };

  if (filename != nullptr) {
    Local<Value> error;
    MaybeLocal<Value> fn = StringBytes::Encode(env->isolate(),
                                               filename,
                                               wrap->encoding_,
                                               &error);
    if (fn.IsEmpty()) {
      argv[0] = Integer::New(env->isolate(), UV_EINVAL);
      argv[2] = StringBytes::Encode(env->isolate(),
                                    filename,
                                    strlen(filename),
                                    BUFFER,
                                    &error).ToLocalChecked();
    } else {
      argv[2] = fn.ToLocalChecked();
    }
  }

  wrap->MakeCallback(env->onchange_string(), arraysize(argv), argv);
}


void FSEventWrap::Close(const FunctionCallbackInfo<Value>& args) {
  FSEventWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());

  if (wrap == nullptr || wrap->initialized_ == false)
    return;
  wrap->initialized_ = false;

  HandleWrap::Close(args);
}

}  // anonymous namespace
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(fs_event_wrap, node::FSEventWrap::Initialize)
