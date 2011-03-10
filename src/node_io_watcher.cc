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

#include <node_io_watcher.h>

#include <node.h>
#include <v8.h>

#include <assert.h>

namespace node {

using namespace v8;

Persistent<FunctionTemplate> IOWatcher::constructor_template;
Persistent<String> callback_symbol;


void IOWatcher::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(IOWatcher::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("IOWatcher"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "start", IOWatcher::Start);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "stop", IOWatcher::Stop);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "set", IOWatcher::Set);

  target->Set(String::NewSymbol("IOWatcher"), constructor_template->GetFunction());

  callback_symbol = NODE_PSYMBOL("callback");
}


void IOWatcher::Callback(EV_P_ ev_io *w, int revents) {
  IOWatcher *io = static_cast<IOWatcher*>(w->data);
  assert(w == &io->watcher_);
  HandleScope scope;

  Local<Value> callback_v = io->handle_->Get(callback_symbol);
  if (!callback_v->IsFunction()) {
    io->Stop();
    return;
  }

  Local<Function> callback = Local<Function>::Cast(callback_v);

  TryCatch try_catch;

  Local<Value> argv[2];
  argv[0] = Local<Value>::New(revents & EV_READ ? True() : False());
  argv[1] = Local<Value>::New(revents & EV_WRITE ? True() : False());

  callback->Call(io->handle_, 2, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


//
//  var io = new process.IOWatcher();
//  process.callback = function (readable, writable) { ... };
//  io.set(fd, true, false);
//  io.start();
//
Handle<Value> IOWatcher::New(const Arguments& args) {
  if (!args.IsConstructCall()) {
    return FromConstructorTemplate(constructor_template, args);
  }

  HandleScope scope;
  IOWatcher *s = new IOWatcher();
  s->Wrap(args.This());
  return args.This();
}


Handle<Value> IOWatcher::Start(const Arguments& args) {
  HandleScope scope;
  IOWatcher *io = ObjectWrap::Unwrap<IOWatcher>(args.Holder());
  io->Start();
  return Undefined();
}


Handle<Value> IOWatcher::Stop(const Arguments& args) {
  HandleScope scope;
  IOWatcher *io = ObjectWrap::Unwrap<IOWatcher>(args.Holder());
  io->Stop();
  return Undefined();
}


void IOWatcher::Start() {
  if (!ev_is_active(&watcher_)) {
    ev_io_start(EV_DEFAULT_UC_ &watcher_);
    Ref();
  }
}


void IOWatcher::Stop() {
  if (ev_is_active(&watcher_)) {
    ev_io_stop(EV_DEFAULT_UC_ &watcher_);
    Unref();
  }
}


Handle<Value> IOWatcher::Set(const Arguments& args) {
  HandleScope scope;

  IOWatcher *io = ObjectWrap::Unwrap<IOWatcher>(args.Holder());

  if (!args[0]->IsInt32()) {
    return ThrowException(Exception::TypeError(
          String::New("First arg should be a file descriptor.")));
  }

  int fd = args[0]->Int32Value();

  if (!args[1]->IsBoolean()) {
    return ThrowException(Exception::TypeError(
          String::New("Second arg should boolean (readable).")));
  }

  int events = 0;

  if (args[1]->IsTrue()) events |= EV_READ;

  if (!args[2]->IsBoolean()) {
    return ThrowException(Exception::TypeError(
          String::New("Third arg should boolean (writable).")));
  }

  if (args[2]->IsTrue()) events |= EV_WRITE;

  assert(!io->watcher_.active);
  ev_io_set(&io->watcher_, fd, events);

  return Undefined();
}



}  // namespace node
