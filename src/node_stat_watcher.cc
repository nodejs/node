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
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "weak-object.h"
#include "weak-object-inl.h"

#include <assert.h>
#include <string.h>
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


void StatWatcher::Initialize(Handle<Object> target) {
  HandleScope scope(node_isolate);

  Local<FunctionTemplate> t = FunctionTemplate::New(StatWatcher::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(FIXED_ONE_BYTE_STRING(node_isolate, "StatWatcher"));

  NODE_SET_PROTOTYPE_METHOD(t, "start", StatWatcher::Start);
  NODE_SET_PROTOTYPE_METHOD(t, "stop", StatWatcher::Stop);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "StatWatcher"),
              t->GetFunction());
}


static void Delete(uv_handle_t* handle) {
  delete reinterpret_cast<uv_fs_poll_t*>(handle);
}


StatWatcher::StatWatcher(Environment* env, Local<Object> wrap)
    : WeakObject(env, wrap),
      watcher_(new uv_fs_poll_t) {
  uv_fs_poll_init(env->event_loop(), watcher_);
  watcher_->data = static_cast<void*>(this);
}


StatWatcher::~StatWatcher() {
  Stop();
  uv_close(reinterpret_cast<uv_handle_t*>(watcher_), Delete);
}


void StatWatcher::Callback(uv_fs_poll_t* handle,
                           int status,
                           const uv_stat_t* prev,
                           const uv_stat_t* curr) {
  StatWatcher* wrap = static_cast<StatWatcher*>(handle->data);
  assert(wrap->watcher_ == handle);
  Environment* env = wrap->env();
  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());
  Local<Value> argv[] = {
    BuildStatsObject(env, curr),
    BuildStatsObject(env, prev),
    Integer::New(status, node_isolate)
  };
  wrap->MakeCallback(env->onchange_string(), ARRAY_SIZE(argv), argv);
}


void StatWatcher::New(const FunctionCallbackInfo<Value>& args) {
  assert(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());
  new StatWatcher(env, args.This());
}


void StatWatcher::Start(const FunctionCallbackInfo<Value>& args) {
  assert(args.Length() == 3);
  HandleScope scope(node_isolate);

  StatWatcher* wrap = Unwrap<StatWatcher>(args.This());
  String::Utf8Value path(args[0]);
  const bool persistent = args[1]->BooleanValue();
  const uint32_t interval = args[2]->Uint32Value();

  if (!persistent)
    uv_unref(reinterpret_cast<uv_handle_t*>(wrap->watcher_));
  uv_fs_poll_start(wrap->watcher_, Callback, *path, interval);
  wrap->ClearWeak();
}


void StatWatcher::Stop(const FunctionCallbackInfo<Value>& args) {
  StatWatcher* wrap = Unwrap<StatWatcher>(args.This());
  Environment* env = wrap->env();
  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());
  wrap->MakeCallback(env->onstop_string(), 0, NULL);
  wrap->Stop();
}


void StatWatcher::Stop() {
  if (!uv_is_active(reinterpret_cast<uv_handle_t*>(watcher_)))
    return;
  uv_fs_poll_stop(watcher_);
  MakeWeak();
}


}  // namespace node
