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
#include "env.h"
#include "node_file.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
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
  t->Inherit(HandleWrap::GetConstructorTemplate(env));

  env->SetProtoMethod(t, "start", StatWatcher::Start);

  target->Set(env->context(), statWatcherString,
              t->GetFunction(env->context()).ToLocalChecked()).FromJust();
}


StatWatcher::StatWatcher(Environment* env,
                         Local<Object> wrap,
                         bool use_bigint)
    : HandleWrap(env,
                 wrap,
                 reinterpret_cast<uv_handle_t*>(&watcher_),
                 AsyncWrap::PROVIDER_STATWATCHER),
      use_bigint_(use_bigint) {
  CHECK_EQ(0, uv_fs_poll_init(env->event_loop(), &watcher_));
}


void StatWatcher::Callback(uv_fs_poll_t* handle,
                           int status,
                           const uv_stat_t* prev,
                           const uv_stat_t* curr) {
  StatWatcher* wrap = ContainerOf(&StatWatcher::watcher_, handle);
  Environment* env = wrap->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> arr = fs::FillGlobalStatsArray(env, wrap->use_bigint_, curr);
  USE(fs::FillGlobalStatsArray(env, wrap->use_bigint_, prev, true));

  Local<Value> argv[2] = { Integer::New(env->isolate(), status), arr };
  wrap->MakeCallback(env->onchange_string(), arraysize(argv), argv);
}


void StatWatcher::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new StatWatcher(env, args.This(), args[0]->IsTrue());
}

// wrap.start(filename, interval)
void StatWatcher::Start(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(args.Length(), 2);

  StatWatcher* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  CHECK(!uv_is_active(wrap->GetHandle()));

  node::Utf8Value path(args.GetIsolate(), args[0]);
  CHECK_NOT_NULL(*path);

  CHECK(args[1]->IsUint32());
  const uint32_t interval = args[1].As<Uint32>()->Value();

  // Note that uv_fs_poll_start does not return ENOENT, we are handling
  // mostly memory errors here.
  const int err = uv_fs_poll_start(&wrap->watcher_, Callback, *path, interval);
  if (err != 0) {
    args.GetReturnValue().Set(err);
  }
}

}  // namespace node
