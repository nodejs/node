// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#include <node_signal_watcher.h>
#include <assert.h>

namespace node {

using namespace v8;

Persistent<FunctionTemplate> SignalWatcher::constructor_template;
static Persistent<String> signal_symbol;

void SignalWatcher::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(SignalWatcher::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(EventEmitter::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("SignalWatcher"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "stop", SignalWatcher::Stop);

  signal_symbol = NODE_PSYMBOL("signal");

  target->Set(String::NewSymbol("SignalWatcher"),
      constructor_template->GetFunction());
}

void SignalWatcher::OnSignal(EV_P_ ev_signal *watcher, int revents) {
  SignalWatcher *w = static_cast<SignalWatcher*>(watcher->data);
  HandleScope scope;

  assert(revents == EV_SIGNAL);

  w->Emit(signal_symbol, 0, NULL);
}

SignalWatcher::~SignalWatcher() {
  if (watcher_.active) {
    ev_ref(EV_DEFAULT_UC);
    ev_signal_stop(EV_DEFAULT_UC_ &watcher_);
  }
}

Handle<Value> SignalWatcher::New(const Arguments& args) {
  HandleScope scope;

  if (args.Length() != 1 || !args[0]->IsInt32()) {
    return ThrowException(String::New("Bad arguments"));
  }

  int sig = args[0]->Int32Value();

  SignalWatcher *w = new SignalWatcher();
  w->Wrap(args.Holder());

  ev_signal_init(&w->watcher_, SignalWatcher::OnSignal, sig);
  w->watcher_.data = w;
  // Give signal handlers very high priority. The only thing that has higher
  // priority is the garbage collector check.
  ev_set_priority(&w->watcher_, EV_MAXPRI-1);
  ev_signal_start(EV_DEFAULT_UC_ &w->watcher_);
  ev_unref(EV_DEFAULT_UC);

  w->Ref();

  return args.This();
}

Handle<Value> SignalWatcher::Stop(const Arguments& args) {
  HandleScope scope;

  SignalWatcher *w = ObjectWrap::Unwrap<SignalWatcher>(args.Holder());
  ev_ref(EV_DEFAULT_UC);
  ev_signal_stop(EV_DEFAULT_UC_ &w->watcher_);
  w->Unref();
  return Undefined();
}

}  // namespace node
