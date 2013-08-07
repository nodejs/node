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

#include <assert.h>
#include <string.h>
#include <stdlib.h>

namespace node {

using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

static Cached<String> onchange_sym;
static Cached<String> onstop_sym;


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


StatWatcher::StatWatcher() : ObjectWrap(),
                             watcher_(new uv_fs_poll_t) {
  uv_fs_poll_init(uv_default_loop(), watcher_);
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
  HandleScope scope(node_isolate);
  Local<Value> argv[3];
  argv[0] = BuildStatsObject(curr);
  argv[1] = BuildStatsObject(prev);
  argv[2] = Integer::New(status, node_isolate);
  if (onchange_sym.IsEmpty()) {
    onchange_sym = FIXED_ONE_BYTE_STRING(node_isolate, "onchange");
  }
  MakeCallback(wrap->handle(node_isolate),
               onchange_sym,
               ARRAY_SIZE(argv),
               argv);
}


void StatWatcher::New(const FunctionCallbackInfo<Value>& args) {
  assert(args.IsConstructCall());
  HandleScope scope(node_isolate);
  StatWatcher* s = new StatWatcher();
  s->Wrap(args.This());
}


void StatWatcher::Start(const FunctionCallbackInfo<Value>& args) {
  assert(args.Length() == 3);
  HandleScope scope(node_isolate);

  StatWatcher* wrap = ObjectWrap::Unwrap<StatWatcher>(args.This());
  String::Utf8Value path(args[0]);
  const bool persistent = args[1]->BooleanValue();
  const uint32_t interval = args[2]->Uint32Value();

  if (!persistent) uv_unref(reinterpret_cast<uv_handle_t*>(wrap->watcher_));
  uv_fs_poll_start(wrap->watcher_, Callback, *path, interval);
  wrap->Ref();
}


void StatWatcher::Stop(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);
  StatWatcher* wrap = ObjectWrap::Unwrap<StatWatcher>(args.This());
  if (onstop_sym.IsEmpty()) {
    onstop_sym = FIXED_ONE_BYTE_STRING(node_isolate, "onstop");
  }
  MakeCallback(wrap->handle(node_isolate), onstop_sym, 0, NULL);
  wrap->Stop();
}


void StatWatcher::Stop() {
  if (!uv_is_active(reinterpret_cast<uv_handle_t*>(watcher_))) return;
  uv_fs_poll_stop(watcher_);
  Unref();
}


}  // namespace node
