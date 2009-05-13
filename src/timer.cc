#include "node.h"
#include "timer.h"
#include <assert.h>

using namespace v8;
using namespace node;

#define CALLBACK_SYMBOL String::NewSymbol("callback")

void
Timer::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> timer_template = FunctionTemplate::New(Timer::New);
  timer_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("Timer"), timer_template->GetFunction());

  NODE_SET_METHOD(timer_template->InstanceTemplate(), "start", Timer::Start);
  NODE_SET_METHOD(timer_template->InstanceTemplate(), "stop", Timer::Stop);
}

void
Timer::OnTimeout (EV_P_ ev_timer *watcher, int revents)
{
  Timer *timer = static_cast<Timer*>(watcher->data);

  HandleScope scope;

  Local<Value> callback_value = timer->handle_->GetHiddenValue(CALLBACK_SYMBOL);
  if (!callback_value->IsFunction()) 
    return;

  Local<Function> callback = Local<Function>::Cast(callback_value);

  TryCatch try_catch;
  callback->Call (timer->handle_, 0, NULL);
  if (try_catch.HasCaught())
    fatal_exception(try_catch);

  /* XXX i'm a bit worried if this is the correct test? 
   * it's rather crutial for memory leaks the conditional here test to see
   * if the watcher will make another callback.
   */ 
  if (!ev_is_active(&timer->watcher_))
    timer->Detach();
}

Timer::Timer (Handle<Object> handle, Handle<Function> callback, ev_tstamp after, ev_tstamp repeat)
  : ObjectWrap(handle)
{
  HandleScope scope;

  handle_->SetHiddenValue(CALLBACK_SYMBOL, callback);

  ev_timer_init(&watcher_, Timer::OnTimeout, after, repeat);
  watcher_.data = this;

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
  timer->Attach();
  return Undefined();
}

Handle<Value>
Timer::Stop (const Arguments& args)
{
  Timer *timer = NODE_UNWRAP(Timer, args.Holder());
  ev_timer_stop(EV_DEFAULT_UC_ &timer->watcher_);
  timer->Detach();
  return Undefined();
}
