#include <node.h>
#include <node_timer.h>
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
  constructor_template->SetClassName(String::NewSymbol("Timer"));

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
  Timer *timer = ObjectWrap::Unwrap<Timer>(info.This());

  assert(timer);
  assert (property == REPEAT_SYMBOL);

  Local<Integer> v = Integer::New(timer->watcher_.repeat);

  return scope.Close(v);
}

void
Timer::RepeatSetter (Local<String> property, Local<Value> value, const AccessorInfo& info)
{
  HandleScope scope;
  Timer *timer = ObjectWrap::Unwrap<Timer>(info.This());

  assert(timer);
  assert(property == REPEAT_SYMBOL);

  timer->watcher_.repeat = NODE_V8_UNIXTIME(value);
}

void
Timer::OnTimeout (EV_P_ ev_timer *watcher, int revents)
{
  Timer *timer = static_cast<Timer*>(watcher->data);

  assert(revents == EV_TIMEOUT);

  timer->Emit("timeout", 0, NULL);

  if (timer->watcher_.repeat == 0) timer->Detach();
}

Timer::~Timer ()
{
  ev_timer_stop (EV_DEFAULT_UC_ &watcher_);
}

Handle<Value>
Timer::New (const Arguments& args)
{
  HandleScope scope;

  Timer *t = new Timer();
  t->Wrap(args.Holder());

  return args.This();
}

Handle<Value>
Timer::Start (const Arguments& args)
{
  Timer *timer = ObjectWrap::Unwrap<Timer>(args.Holder());
  HandleScope scope;

  if (args.Length() != 2)
    return ThrowException(String::New("Bad arguments"));

  ev_tstamp after = NODE_V8_UNIXTIME(args[0]);
  ev_tstamp repeat = NODE_V8_UNIXTIME(args[1]);

  ev_timer_init(&timer->watcher_, Timer::OnTimeout, after, repeat);
  timer->watcher_.data = timer;
  ev_timer_start(EV_DEFAULT_UC_ &timer->watcher_);

  timer->Attach();

  return Undefined();
}

Handle<Value>
Timer::Stop (const Arguments& args)
{
  Timer *timer = ObjectWrap::Unwrap<Timer>(args.Holder());
  if (ev_is_active(&timer->watcher_)) {
    ev_timer_stop(EV_DEFAULT_UC_ &timer->watcher_);
    timer->Detach();
  }
  return Undefined();
}
