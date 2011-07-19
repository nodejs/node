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

#include <node_stat_watcher.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

namespace node {

using namespace v8;

Persistent<FunctionTemplate> StatWatcher::constructor_template;

void StatWatcher::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(StatWatcher::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("StatWatcher"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "start", StatWatcher::Start);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "stop", StatWatcher::Stop);

  target->Set(String::NewSymbol("StatWatcher"), constructor_template->GetFunction());
}


void StatWatcher::Callback(EV_P_ ev_stat *watcher, int revents) {
  assert(revents == EV_STAT);
  StatWatcher *handler = static_cast<StatWatcher*>(watcher->data);
  assert(watcher == &handler->watcher_);
  HandleScope scope;
  Handle<Value> argv[2];
  argv[0] = Handle<Value>(BuildStatsObject(&watcher->attr));
  argv[1] = Handle<Value>(BuildStatsObject(&watcher->prev));
  MakeCallback(handler->handle_, "onchange", 2, argv);
}


Handle<Value> StatWatcher::New(const Arguments& args) {
  if (!args.IsConstructCall()) {
    return FromConstructorTemplate(constructor_template, args);
  }

  HandleScope scope;
  StatWatcher *s = new StatWatcher();
  s->Wrap(args.Holder());
  return args.This();
}


Handle<Value> StatWatcher::Start(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New("Bad arguments")));
  }

  StatWatcher *handler = ObjectWrap::Unwrap<StatWatcher>(args.Holder());
  String::Utf8Value path(args[0]->ToString());

  assert(handler->path_ == NULL);
  handler->path_ = strdup(*path);

  ev_tstamp interval = 0.;
  if (args[2]->IsInt32()) {
    interval = NODE_V8_UNIXTIME(args[2]);
  }

  ev_stat_set(&handler->watcher_, handler->path_, interval);
  ev_stat_start(EV_DEFAULT_UC_ &handler->watcher_);

  handler->persistent_ = args[1]->IsTrue();

  if (!handler->persistent_) {
    ev_unref(EV_DEFAULT_UC);
  }

  handler->Ref();

  return Undefined();
}


Handle<Value> StatWatcher::Stop(const Arguments& args) {
  HandleScope scope;
  StatWatcher *handler = ObjectWrap::Unwrap<StatWatcher>(args.Holder());
  MakeCallback(handler->handle_, "onstop", 0, NULL);
  handler->Stop();
  return Undefined();
}


void StatWatcher::Stop () {
  if (watcher_.active) {
    if (!persistent_) ev_ref(EV_DEFAULT_UC);
    ev_stat_stop(EV_DEFAULT_UC_ &watcher_);
    free(path_);
    path_ = NULL;
    Unref();
  }
}


}  // namespace node
