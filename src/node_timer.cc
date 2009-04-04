#include "node.h"
#include "node_timer.h"
#include <assert.h>

using namespace v8;

class Timer {
 public:
  Timer(Handle<Function> callback, int argc, Handle<Value> argv[], ev_tstamp after, ev_tstamp repeat);
  ~Timer();
  Local<External> CreateTimeoutID ();
  void CallCallback ();
 private:
  ev_timer watcher;
  Persistent<External> timeoutID;
  Persistent<Function> callback;
  int argc;
  Persistent<Value> argv[];
};

static void
onTimeout (struct ev_loop *loop, ev_timer *watcher, int revents)
{
  Timer *timer = static_cast<Timer*>(watcher->data);

  timer->CallCallback();

  // use ev_is_active instead?
  if(watcher->repeat == 0.) 
    delete timer;
}

Timer::Timer (Handle<Function> _callback, int _argc, Handle<Value> _argv[], ev_tstamp after, ev_tstamp repeat)
{
  HandleScope scope;
  callback = Persistent<Function>::New(_callback);
  argc = _argc;

  ev_timer_init (&watcher, onTimeout, after, repeat);
  watcher.data = this;
  ev_timer_start (node_loop(), &watcher);
}

Timer::~Timer ()
{
  ev_timer_stop (node_loop(), &watcher);

  callback.Dispose();

  timeoutID.Dispose();
  timeoutID.Clear();
}

void
Timer::CallCallback ()
{
  HandleScope scope;

  TryCatch try_catch;

  callback->Call (Context::GetCurrent()->Global(), argc, argv);

  if(try_catch.HasCaught())
    node_fatal_exception(try_catch);
}

Local<External>
Timer::CreateTimeoutID ()
{
  HandleScope scope;

  Local<External> timeoutID_local = External::New(this);

  timeoutID = Persistent<External>::New(timeoutID_local);

  return scope.Close(timeoutID_local);
}

static Timer *
UnwrapTimeoutID (Handle<External> timeoutID)
{
  HandleScope scope;

  Timer *timer = static_cast<Timer*>(timeoutID->Value());

  return timer;
}

// timeoutID = setTimeout(func, delay, [param1, param2, ...]);
// timeoutID = setTimeout(code, delay);
// 
// * timeoutID is the ID of the timeout, which can be used with
//   clearTimeout.
//
// * func is the function you want to execute after delay milliseconds.
//
// * code in the alternate syntax, is a string of code you want to execute
//   after delay milliseconds. (not recommended)
//
// * delay is the number of milliseconds (thousandths of a second) that the
//   function call should be delayed by. 
static Handle<Value>
setTimeout(const Arguments& args) 
{
  if (args.Length() < 2)
    return Undefined();

  HandleScope scope;

  Local<Function> callback = Local<Function>::Cast(args[0]);
  uint32_t delay = args[1]->Uint32Value();

  ev_tstamp after = (double)delay / 1000.0;

  if (args.Length() > 2)
    assert(0 && "extra params to setTimeout not yet implemented.");
  int argc = 0;
  Handle<Value> argv[] = {};
  /*
  int argc = args.Length() - 2;
  Handle<Value> argv[] = new Handle<Value>[argc];
  // the rest of the arguments, if any arg parameters for the callback
  for(int i = 2; i < args.Length(); i++)
    argv[i - 2] = args[i];
  */

  Timer *timer = new Timer(callback, argc, argv, after, 0.0);

  Local<External> timeoutID = timer->CreateTimeoutID();

  return scope.Close(timeoutID);
}

// clearTimeout(timeoutID)
static Handle<Value> clearTimeout
  ( const Arguments& args
  ) 
{
  if (args.Length() < 1)
    return Undefined();

  Handle<External> timeoutID = Handle<External>::Cast(args[0]);
  Timer *timer = UnwrapTimeoutID(timeoutID);

  delete timer;

  return Undefined();
}

// intervalID = setInterval(func, delay[, param1, param2, ...]);
// intervalID = setInterval(code, delay);
// 
// where
// 
// * intervalID is a unique interval ID you can pass to clearInterval().
//
// * func is the function you want to be called repeatedly.
//
// * code in the alternate syntax, is a string of code you want to be executed
//   repeatedly.
//
// * delay is the number of milliseconds (thousandths of a second) that the
//   setInterval() function should wait before each call to func.
static Handle<Value> setInterval
  ( const Arguments& args
  ) 
{
  if (args.Length() < 2)
    return Undefined();

  HandleScope scope;

  Local<Function> callback = Local<Function>::Cast(args[0]);
  uint32_t delay = args[1]->Uint32Value();

  ev_tstamp after = (double)delay / 1000.0;

  if (args.Length() > 2)
    assert(0 && "extra params to setInterval not yet implemented.");
  int argc = 0;
  Handle<Value> argv[] = {};

  Timer *timer = new Timer(callback, argc, argv, after, after);

  Local<External> timeoutID = timer->CreateTimeoutID();

  return scope.Close(timeoutID);
}

void
Init_timer (Handle<Object> target)
{
  HandleScope scope;

  target->Set ( String::New("setTimeout")
              , FunctionTemplate::New(setTimeout)->GetFunction()
              );

  target->Set ( String::New("clearTimeout")
              , FunctionTemplate::New(clearTimeout)->GetFunction()
              );

  target->Set ( String::New("setInterval")
              , FunctionTemplate::New(setInterval)->GetFunction()
              );

  target->Set ( String::New("clearInterval")
              , FunctionTemplate::New(clearTimeout)->GetFunction()
              );
}
