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

#include <node_signal_watcher.h>
#include <assert.h>

namespace node {

using namespace v8;

Persistent<FunctionTemplate> SignalWatcher::constructor_template;
static Persistent<String> callback_symbol;

void SignalWatcher::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(SignalWatcher::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("SignalWatcher"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "start", SignalWatcher::Start);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "stop", SignalWatcher::Stop);

  target->Set(String::NewSymbol("SignalWatcher"),
      constructor_template->GetFunction());

  callback_symbol = NODE_PSYMBOL("callback");
}

void SignalWatcher::Callback(EV_P_ ev_signal *watcher, int revents) {
  SignalWatcher *w = static_cast<SignalWatcher*>(watcher->data);

  assert(watcher == &w->watcher_);
  assert(revents == EV_SIGNAL);

  HandleScope scope;

  Local<Value> callback_v = w->handle_->Get(callback_symbol);
  if (!callback_v->IsFunction()) {
    w->Stop();
    return;
  }

  Local<Function> callback = Local<Function>::Cast(callback_v);

  TryCatch try_catch;

  callback->Call(w->handle_, 0, NULL);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}

Handle<Value> SignalWatcher::New(const Arguments& args) {
  if (!args.IsConstructCall()) {
    return FromConstructorTemplate(constructor_template, args);
  }

  HandleScope scope;

  if (args.Length() != 1 || !args[0]->IsInt32()) {
    return ThrowException(String::New("Bad arguments"));
  }

  int sig = args[0]->Int32Value();

  SignalWatcher *w = new SignalWatcher(sig);
  w->Wrap(args.Holder());

  return args.This();
}

Handle<Value> SignalWatcher::Start(const Arguments& args) {
  HandleScope scope;
  SignalWatcher *w = ObjectWrap::Unwrap<SignalWatcher>(args.Holder());
  w->Start();
  return Undefined();
}

void SignalWatcher::Start () {
  if (!watcher_.active) {
    ev_signal_start(EV_DEFAULT_UC_ &watcher_);
    ev_unref(EV_DEFAULT_UC);
    Ref();
  }
}

Handle<Value> SignalWatcher::Stop(const Arguments& args) {
  HandleScope scope;
  SignalWatcher *w = ObjectWrap::Unwrap<SignalWatcher>(args.Holder());
  w->Stop();
  return Undefined();
}

void SignalWatcher::Stop () {
  if (watcher_.active) {
    ev_ref(EV_DEFAULT_UC);
    ev_signal_stop(EV_DEFAULT_UC_ &watcher_);
    Unref();
  }
}

}  // namespace node

NODE_MODULE(node_signal_watcher, node::SignalWatcher::Initialize)
