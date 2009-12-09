// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#include <node_stat.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

namespace node {

using namespace v8;

Persistent<FunctionTemplate> Stat::constructor_template;

static Persistent<String> change_symbol;
static Persistent<String> stop_symbol;

void Stat::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Stat::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(EventEmitter::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Stat"));

  change_symbol = NODE_PSYMBOL("change");
  stop_symbol = NODE_PSYMBOL("stop");

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "start", Stat::Start);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "stop", Stat::Stop);

  target->Set(String::NewSymbol("Stat"), constructor_template->GetFunction());
}


void Stat::Callback(EV_P_ ev_stat *watcher, int revents) {
  assert(revents == EV_STAT);
  Stat *handler = static_cast<Stat*>(watcher->data);
  assert(watcher == &handler->watcher_);
  HandleScope scope;
  Handle<Value> argv[2];
  argv[0] = Handle<Value>(BuildStatsObject(&watcher->attr));
  argv[1] = Handle<Value>(BuildStatsObject(&watcher->prev));
  handler->Emit(change_symbol, 2, argv);
}


Handle<Value> Stat::New(const Arguments& args) {
  HandleScope scope;
  Stat *s = new Stat();
  s->Wrap(args.Holder());
  return args.This();
}


Handle<Value> Stat::Start(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New("Bad arguments")));
  }

  Stat *handler = ObjectWrap::Unwrap<Stat>(args.Holder());
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


Handle<Value> Stat::Stop(const Arguments& args) {
  HandleScope scope;
  Stat *handler = ObjectWrap::Unwrap<Stat>(args.Holder());
  handler->Emit(stop_symbol, 0, NULL);
  handler->Stop();
  return Undefined();
}


void Stat::Stop () {
  if (watcher_.active) {
    if (!persistent_) ev_ref(EV_DEFAULT_UC);
    ev_stat_stop(EV_DEFAULT_UC_ &watcher_);
    free(path_);
    path_ = NULL;
    Unref();
  }
}


}  // namespace node
