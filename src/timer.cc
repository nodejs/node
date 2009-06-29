#include "node.h"
#include "timer.h"
#include <assert.h>

using namespace v8;
using namespace node;

#define REPEAT_SYMBOL   String::NewSymbol("repeat")

Persistent<FunctionTemplate> Timer::constructor_template;

void
Timer::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Timer::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(EventEmitter::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "start", Timer::Start);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "stop", Timer::Stop);

  constructor_template->PrototypeTemplate()->SetAccessor(REPEAT_SYMBOL,
      RepeatGetter, RepeatSetter);

  target->Set(String::NewSymbol("Timer"), constructor_template->GetFunction());
}

Handle<Value>
Timer::RepeatGetter (Local<String> property, const AccessorInfo& info)
{
  HandleScope scope;
  Timer *timer = NODE_UNWRAP(Timer, info.This());

  assert(timer);
  assert (property == REPEAT_SYMBOL);

  Local<Integer> v = Integer::New(timer->watcher_.repeat);

  return scope.Close(v);
}

void 
Timer::RepeatSetter (Local<String> property, Local<Value> value, const AccessorInfo& info)
{
  HandleScope scope;
  Timer *timer = NODE_UNWRAP(Timer, info.This());

  assert(timer);
  assert(property == REPEAT_SYMBOL);

  timer->watcher_.repeat = (double)(value->IntegerValue()) / 1000;
}

void
Timer::OnTimeout (EV_P_ ev_timer *watcher, int revents)
{
  Timer *timer = static_cast<Timer*>(watcher->data);

  assert(revents == EV_TIMEOUT);

  timer->Emit("timeout", 0, NULL);

  /* XXX i'm a bit worried if this is the correct test? 
   * it's rather crutial for memory leaks the conditional here test to see
   * if the watcher will make another callback.
   */ 
  if (!ev_is_active(&timer->watcher_))
    timer->Detach();
}

Timer::~Timer ()
{
  ev_timer_stop (EV_DEFAULT_UC_ &watcher_);
}

Handle<Value>
Timer::New (const Arguments& args)
{
  HandleScope scope;

  Timer *t = new Timer(args.Holder());
  ObjectWrap::InformV8ofAllocation(t);

  return args.This();
}

Handle<Value>
Timer::Start (const Arguments& args)
{
  Timer *timer = NODE_UNWRAP(Timer, args.Holder());
  HandleScope scope;

  if (args.Length() != 2)
    return ThrowException(String::New("Bad arguments"));

  ev_tstamp after = (double)(args[0]->IntegerValue())  / 1000.0;
  ev_tstamp repeat = (double)(args[1]->IntegerValue())  / 1000.0;

  ev_timer_init(&timer->watcher_, Timer::OnTimeout, after, repeat);
  timer->watcher_.data = timer;
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
