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

#include "node_stat_watcher.h"
#include "async_wrap-inl.h"
#include "env-inl.h"
#include "util-inl.h"

#include <string.h>
#include <stdlib.h>

namespace node {

using v8::Context;
using v8::DontDelete;
using v8::DontEnum;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::Signature;
using v8::String;
using v8::Uint32;
using v8::Value;


void StatWatcher::Initialize(Environment* env, Local<Object> target) {
  HandleScope scope(env->isolate());

  Local<FunctionTemplate> t = env->NewFunctionTemplate(StatWatcher::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  Local<String> statWatcherString =
      FIXED_ONE_BYTE_STRING(env->isolate(), "StatWatcher");
  t->SetClassName(statWatcherString);

  AsyncWrap::AddWrapMethods(env, t);
  env->SetProtoMethod(t, "start", StatWatcher::Start);
  env->SetProtoMethod(t, "stop", StatWatcher::Stop);

  Local<FunctionTemplate> is_active_templ =
      FunctionTemplate::New(env->isolate(),
                            IsActive,
                            env->as_external(),
                            Signature::New(env->isolate(), t));
  t->PrototypeTemplate()->SetAccessorProperty(
      FIXED_ONE_BYTE_STRING(env->isolate(), "isActive"),
      is_active_templ,
      Local<FunctionTemplate>(),
      static_cast<PropertyAttribute>(ReadOnly | DontDelete | DontEnum));

  target->Set(statWatcherString, t->GetFunction());
}


static void Delete(uv_handle_t* handle) {
  delete reinterpret_cast<uv_fs_poll_t*>(handle);
}


StatWatcher::StatWatcher(Environment* env, Local<Object> wrap)
    : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_STATWATCHER),
      watcher_(new uv_fs_poll_t) {
  MakeWeak<StatWatcher>(this);
  Wrap(wrap, this);
  uv_fs_poll_init(env->event_loop(), watcher_);
  watcher_->data = static_cast<void*>(this);
}


StatWatcher::~StatWatcher() {
  if (IsActive()) {
    Stop();
  }
  uv_close(reinterpret_cast<uv_handle_t*>(watcher_), Delete);
}


void StatWatcher::Callback(uv_fs_poll_t* handle,
                           int status,
                           const uv_stat_t* prev,
                           const uv_stat_t* curr) {
  StatWatcher* wrap = static_cast<StatWatcher*>(handle->data);
  CHECK_EQ(wrap->watcher_, handle);
  Environment* env = wrap->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  node::FillGlobalStatsArray(env, curr);
  node::FillGlobalStatsArray(env, prev, env->kFsStatsFieldsLength);

  Local<Value> argv[2] {
    Integer::New(env->isolate(), status),
    env->fs_stats_field_array()->GetJSArray()
  };
  wrap->MakeCallback(env->onchange_string(), arraysize(argv), argv);
}


void StatWatcher::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new StatWatcher(env, args.This());
}

bool StatWatcher::IsActive() {
  return uv_is_active(reinterpret_cast<uv_handle_t*>(watcher_)) != 0;
}

void StatWatcher::IsActive(const v8::FunctionCallbackInfo<v8::Value>& args) {
  StatWatcher* wrap = Unwrap<StatWatcher>(args.This());
  CHECK(wrap != nullptr);
  args.GetReturnValue().Set(wrap->IsActive());
}

// wrap.start(filename, persistent, interval)
void StatWatcher::Start(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(args.Length(), 3);

  StatWatcher* wrap = Unwrap<StatWatcher>(args.Holder());
  CHECK_NE(wrap, nullptr);
  if (wrap->IsActive()) {
    return;
  }

  const int argc = args.Length();
  CHECK_GE(argc, 3);

  node::Utf8Value path(args.GetIsolate(), args[0]);
  CHECK_NE(*path, nullptr);

  bool persistent = true;
  if (args[1]->IsFalse()) {
    persistent = false;
  }

  CHECK(args[2]->IsUint32());
  const uint32_t interval = args[2].As<Uint32>()->Value();

  // Safe, uv_ref/uv_unref are idempotent.
  if (persistent)
    uv_ref(reinterpret_cast<uv_handle_t*>(wrap->watcher_));
  else
    uv_unref(reinterpret_cast<uv_handle_t*>(wrap->watcher_));

  // Note that uv_fs_poll_start does not return ENOENT, we are handling
  // mostly memory errors here.
  const int err = uv_fs_poll_start(wrap->watcher_, Callback, *path, interval);
  if (err != 0) {
    args.GetReturnValue().Set(err);
  }
  wrap->ClearWeak();
}


void StatWatcher::Stop(const FunctionCallbackInfo<Value>& args) {
  StatWatcher* wrap = Unwrap<StatWatcher>(args.Holder());
  CHECK_NE(wrap, nullptr);
  if (!wrap->IsActive()) {
    return;
  }

  Environment* env = wrap->env();
  Context::Scope context_scope(env->context());
  wrap->MakeCallback(env->onstop_string(), 0, nullptr);
  wrap->Stop();
}


void StatWatcher::Stop() {
  uv_fs_poll_stop(watcher_);
  MakeWeak<StatWatcher>(this);
}


}  // namespace node
