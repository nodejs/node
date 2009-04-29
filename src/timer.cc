#include "node.h"
#include "timer.h"
#include <assert.h>

using namespace v8;

class Timer : node::ObjectWrap {
 public:
  Timer(Handle<Object> handle, Handle<Function> callback, ev_tstamp after, ev_tstamp repeat);
  ~Timer();

  static Handle<Value> New (const Arguments& args);
  static Handle<Value> Start (const Arguments& args);
  static Handle<Value> Stop (const Arguments& args);

 private:
  static void OnTimeout (EV_P_ ev_timer *watcher, int revents);
  ev_timer watcher_;
};

void
Timer::OnTimeout (EV_P_ ev_timer *watcher, int revents)
{
  Timer *timer = static_cast<Timer*>(watcher->data);

  HandleScope scope;

  Local<Value> callback_value = timer->handle_->Get(String::NewSymbol("callback"));
  if (!callback_value->IsFunction()) 
    return;

  Local<Function> callback = Local<Function>::Cast(callback_value);

  TryCatch try_catch;
  callback->Call (timer->handle_, 0, NULL);
  if (try_catch.HasCaught())
    node::fatal_exception(try_catch);
}

Timer::Timer (Handle<Object> handle, Handle<Function> callback, ev_tstamp after, ev_tstamp repeat)
  : ObjectWrap(handle)
{
  HandleScope scope;

  handle_->Set(String::NewSymbol("callback"), callback);

  ev_timer_init(&watcher_, Timer::OnTimeout, after, repeat);
  watcher_.data = this;

  ev_timer_start(EV_DEFAULT_UC_ &watcher_);
}

Timer::~Timer ()
{
  ev_timer_stop (EV_DEFAULT_UC_ &watcher_);
}

Handle<Value>
Timer::New (const Arguments& args)
{
  if (args.Length() < 2)
    return Undefined();

  HandleScope scope;

  Local<Function> callback = Local<Function>::Cast(args[0]);

  ev_tstamp after = (double)(args[1]->IntegerValue())  / 1000.0;
  ev_tstamp repeat = (double)(args[2]->IntegerValue())  / 1000.0;

  new Timer(args.Holder(), callback, after, repeat);

  return args.This();
}

Handle<Value>
Timer::Start (const Arguments& args)
{
  Timer *timer = NODE_UNWRAP(Timer, args.Holder());
  ev_timer_start(EV_DEFAULT_UC_ &timer->watcher_);
  return Undefined();
}

Handle<Value>
Timer::Stop (const Arguments& args)
{
  Timer *timer = NODE_UNWRAP(Timer, args.Holder());
  ev_timer_stop(EV_DEFAULT_UC_ &timer->watcher_);
  return Undefined();
}

void
node::Init_timer (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> timer_template = FunctionTemplate::New(Timer::New);
  timer_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("Timer"), timer_template->GetFunction());

  NODE_SET_METHOD(timer_template->InstanceTemplate(), "start", Timer::Start);
  NODE_SET_METHOD(timer_template->InstanceTemplate(), "stop", Timer::Stop);
}
