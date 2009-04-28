#include "node.h"
#include "timers.h"
#include <assert.h>

using namespace v8;

static Persistent<ObjectTemplate> timer_template;

class Timer {
 public:
  Timer(Handle<Function> callback, ev_tstamp after, ev_tstamp repeat);
  ~Timer();

  static Handle<Value> setTimeout (const Arguments& args);
  static Handle<Value> setInterval (const Arguments& args);
  static Handle<Value> clearTimeout (const Arguments& args);

  Persistent<Object> handle_;

 private:
  static Timer* Unwrap (Handle<Object> handle);
  static void OnTimeout (EV_P_ ev_timer *watcher, int revents);
  ev_timer watcher_;
};

Timer* 
Timer::Unwrap (Handle<Object> handle)
{
  HandleScope scope;
  Handle<External> field = Handle<External>::Cast(handle->GetInternalField(0));
  Timer* timer = static_cast<Timer*>(field->Value());
  return timer;
}

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
  callback->Call (Context::GetCurrent()->Global(), 0, NULL);
  if(try_catch.HasCaught())
    node::fatal_exception(try_catch);

  // use ev_is_active instead?
  if(watcher->repeat == 0.) 
    delete timer;
}

Timer::Timer (Handle<Function> callback, ev_tstamp after, ev_tstamp repeat)
{
  HandleScope scope;

  handle_ = Persistent<Object>::New(timer_template->NewInstance());
  handle_->Set(String::NewSymbol("callback"), callback);

  Local<External> external = External::New(this);
  handle_->SetInternalField(0, external);

  ev_timer_init(&watcher_, Timer::OnTimeout, after, repeat);
  watcher_.data = this;

  ev_timer_start(EV_DEFAULT_UC_ &watcher_);
}

Timer::~Timer ()
{
  ev_timer_stop (EV_DEFAULT_UC_ &watcher_);
  handle_->SetInternalField(0, Undefined());
  handle_.Dispose();
}

Handle<Value>
Timer::setTimeout (const Arguments& args)
{
  if (args.Length() < 2)
    return Undefined();

  HandleScope scope;

  Local<Function> callback = Local<Function>::Cast(args[0]);
  int delay = args[1]->IntegerValue();

  ev_tstamp after = (double)delay / 1000.0;

  if (args.Length() > 2)
    assert(0 && "extra params to setTimeout not yet implemented.");

  Timer *timer = new Timer(callback, after, 0.0);

  return scope.Close(timer->handle_);
}

Handle<Value>
Timer::setInterval (const Arguments& args)
{
  if (args.Length() < 2)
    return Undefined();

  HandleScope scope;

  Local<Function> callback = Local<Function>::Cast(args[0]);
  int delay = args[1]->IntegerValue();

  ev_tstamp after = (double)delay / 1000.0;

  Timer *timer = new Timer(callback, after, after);

  return scope.Close(timer->handle_);
}

Handle<Value>
Timer::clearTimeout (const Arguments& args)
{
  if (args.Length() < 1)
    return Undefined();

  HandleScope scope;
  Local<Object> handle = Local<Object>::Cast(args[0]);
  Timer *timer = Timer::Unwrap(handle);
  delete timer;

  return Undefined();
}

void
node::Init_timers (Handle<Object> target)
{
  HandleScope scope;

  timer_template = Persistent<ObjectTemplate>::New(ObjectTemplate::New());
  timer_template->SetInternalFieldCount(1);

  NODE_SET_METHOD(target, "setTimeout", Timer::setTimeout);
  NODE_SET_METHOD(target, "setInterval", Timer::setInterval);
  NODE_SET_METHOD(target, "clearTimeout", Timer::clearTimeout);
  NODE_SET_METHOD(target, "clearInterval", Timer::clearTimeout);
}
