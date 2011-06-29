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

#include <node.h>
#include <node_timer.h>
#include <assert.h>

namespace node {

using namespace v8;

Persistent<FunctionTemplate> Timer::constructor_template;


static Persistent<String> timeout_symbol;
static Persistent<String> repeat_symbol;
static Persistent<String> callback_symbol;


void Timer::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Timer::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Timer"));

  timeout_symbol = NODE_PSYMBOL("timeout");
  repeat_symbol = NODE_PSYMBOL("repeat");
  callback_symbol = NODE_PSYMBOL("callback");

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "start", Timer::Start);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "stop", Timer::Stop);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "again", Timer::Again);

  constructor_template->InstanceTemplate()->SetAccessor(repeat_symbol,
      RepeatGetter, RepeatSetter);

  target->Set(String::NewSymbol("Timer"), constructor_template->GetFunction());
}


Handle<Value> Timer::RepeatGetter(Local<String> property,
                                  const AccessorInfo& info) {
  HandleScope scope;
  Timer *timer = ObjectWrap::Unwrap<Timer>(info.This());

  assert(timer);
  assert(property == repeat_symbol);

  Local<Integer> v = Integer::New(timer->watcher_.repeat);

  return scope.Close(v);
}

void Timer::RepeatSetter(Local<String> property,
                         Local<Value> value,
                         const AccessorInfo& info) {
  HandleScope scope;
  Timer *timer = ObjectWrap::Unwrap<Timer>(info.This());

  assert(timer);
  assert(property == repeat_symbol);

  timer->watcher_.repeat = NODE_V8_UNIXTIME(value);
}

void Timer::OnTimeout(EV_P_ ev_timer *watcher, int revents) {
  Timer *timer = static_cast<Timer*>(watcher->data);

  assert(revents == EV_TIMEOUT);

  HandleScope scope;

  Local<Value> callback_v = timer->handle_->Get(callback_symbol);
  if (!callback_v->IsFunction()) {
    timer->Stop();
    return;
  }

  Local<Function> callback = Local<Function>::Cast(callback_v);

  TryCatch try_catch;

  callback->Call(timer->handle_, 0, NULL);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  if (timer->watcher_.repeat == 0) timer->Unref();
}


Timer::~Timer() {
  ev_timer_stop(EV_DEFAULT_UC_ &watcher_);
}


Handle<Value> Timer::New(const Arguments& args) {
  if (!args.IsConstructCall()) {
    return FromConstructorTemplate(constructor_template, args);
  }

  HandleScope scope;

  Timer *t = new Timer();
  t->Wrap(args.Holder());

  return args.This();
}

Handle<Value> Timer::Start(const Arguments& args) {
  HandleScope scope;
  Timer *timer = ObjectWrap::Unwrap<Timer>(args.Holder());

  if (args.Length() != 2)
    return ThrowException(String::New("Bad arguments"));

  bool was_active = ev_is_active(&timer->watcher_);

  ev_tstamp after = NODE_V8_UNIXTIME(args[0]);
  ev_tstamp repeat = NODE_V8_UNIXTIME(args[1]);
  ev_timer_init(&timer->watcher_, Timer::OnTimeout, after, repeat);
  timer->watcher_.data = timer;

  // Update the event loop time. Need to call this because processing JS can
  // take non-negligible amounts of time.
  ev_now_update(EV_DEFAULT_UC);

  ev_timer_start(EV_DEFAULT_UC_ &timer->watcher_);

  if (!was_active) timer->Ref();

  return Undefined();
}


Handle<Value> Timer::Stop(const Arguments& args) {
  HandleScope scope;
  Timer *timer = ObjectWrap::Unwrap<Timer>(args.Holder());
  timer->Stop();
  return Undefined();
}


void Timer::Stop() {
  if (watcher_.active) {
    ev_timer_stop(EV_DEFAULT_UC_ &watcher_);
    Unref();
  }
}


Handle<Value> Timer::Again(const Arguments& args) {
  HandleScope scope;
  Timer *timer = ObjectWrap::Unwrap<Timer>(args.Holder());

  int was_active = ev_is_active(&timer->watcher_);

  if (args.Length() > 0) {
    ev_tstamp repeat = NODE_V8_UNIXTIME(args[0]);
    if (repeat > 0) timer->watcher_.repeat = repeat;
  }

  ev_timer_again(EV_DEFAULT_UC_ &timer->watcher_);

  // ev_timer_again can start or stop the watcher.
  // So we need to check what happened and adjust the ref count
  // appropriately.

  if (ev_is_active(&timer->watcher_)) {
    if (!was_active) timer->Ref();
  } else {
    if (was_active) timer->Unref();
  }

  return Undefined();
}


}  // namespace node
