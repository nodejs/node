#include "events.h"
#include <ev.h>
#include <v8.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> /* inet_ntop */
#include <netinet/in.h> /* sockaddr_in, sockaddr_in6 */

#ifndef RAMP
# define RAMP(x) ((x) > 0 ? (x) : 0)
#endif

using namespace v8;
using namespace node;

Persistent<FunctionTemplate> EventEmitter::constructor_template;

void 
EventEmitter::Initialize (Local<FunctionTemplate> ctemplate)
{
  HandleScope scope;

  constructor_template = Persistent<FunctionTemplate>::New(ctemplate);

  Local<FunctionTemplate> __emit = FunctionTemplate::New(Emit);
  constructor_template->PrototypeTemplate()->Set(String::NewSymbol("emit"), __emit);

  // All other prototype methods are defined in events.js
}

static bool
ReallyEmit (Handle<Object> self, Handle<String> event, int argc, Handle<Value> argv[])
{
  HandleScope scope; 

  Local<Value> events_v = self->Get(String::NewSymbol("_events"));
  if (!events_v->IsObject()) return false;
  Local<Object> events = events_v->ToObject();

  Local<Value> listeners_v = events->Get(event);
  if (!listeners_v->IsArray()) return false;
  Local<Array> listeners = Local<Array>::Cast(listeners_v);

  for (unsigned int i = 0; i < listeners->Length(); i++) {
    HandleScope scope; 

    Local<Value> listener_v = listeners->Get(Integer::New(i));
    if (!listener_v->IsFunction()) continue;
    Local<Function> listener = Local<Function>::Cast(listener_v);

    TryCatch try_catch;

    listener->Call(self, argc, argv);

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
      return false;
    }
  }

  return true;
}

Handle<Value>
EventEmitter::Emit (const Arguments& args)
{
  HandleScope scope; 
  Local<String> event = args[0]->ToString();
  int argc = 0;
  Local<Array> emit_args;
  if (args[1]->IsArray()) {
    emit_args = Local<Array>::Cast(args[1]);
    argc = emit_args->Length();
  }
  Local<Value> argv[argc];

  for (int i = 0; i < argc; i++) {
    argv[i] = emit_args->Get(Integer::New(i));
  }

  bool r = ReallyEmit(args.Holder(), event, argc, argv); 

  return scope.Close(r ? True() : False());
}

bool
EventEmitter::Emit (const char *event_s, int argc, Handle<Value> argv[])
{
  HandleScope scope; 
  Local<String> event = String::NewSymbol(event_s);
  return ReallyEmit(handle_, event, argc, argv);
}

Persistent<FunctionTemplate> Promise::constructor_template;

void 
Promise::Initialize (v8::Handle<v8::Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(EventEmitter::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "block", Block);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "emitSuccess", EmitSuccess);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "emitError", EmitError);

  target->Set(String::NewSymbol("Promise"),
              constructor_template->GetFunction());
}

v8::Handle<v8::Value>
Promise::New (const v8::Arguments& args)
{
  HandleScope scope;

  Promise *promise = new Promise();
  promise->Wrap(args.This());
  promise->Attach();

  return args.This();
}

Handle<Value>
Promise::Block (const Arguments& args)
{
  HandleScope scope;
  Promise *promise = ObjectWrap::Unwrap<Promise>(args.Holder());
  promise->Block();
  return Undefined();
}

v8::Handle<v8::Value>
Promise::EmitSuccess (const v8::Arguments& args)
{
  HandleScope scope;
  Promise *promise = ObjectWrap::Unwrap<Promise>(args.Holder());

  int argc = 0;
  Local<Array> emit_args;
  if (args[0]->IsArray()) {
    emit_args = Local<Array>::Cast(args[0]);
    argc = emit_args->Length();
  }
  Local<Value> argv[argc];
  for (int i = 0; i < argc; i++) {
    argv[i] = emit_args->Get(Integer::New(i));
  }

  bool r = promise->EmitSuccess(argc, argv);

  return r ? True() : False();
}

v8::Handle<v8::Value>
Promise::EmitError (const v8::Arguments& args)
{
  HandleScope scope;
  Promise *promise = ObjectWrap::Unwrap<Promise>(args.Holder());

  int argc = 0;
  Local<Array> emit_args;
  if (args[0]->IsArray()) {
    emit_args = Local<Array>::Cast(args[0]);
    argc = emit_args->Length();
  }
  Local<Value> argv[argc];
  for (int i = 0; i < argc; i++) {
    argv[i] = emit_args->Get(Integer::New(i));
  }

  bool r = promise->EmitError(argc, argv);

  return r ? True() : False();
}

void
Promise::Block (void)
{
  blocking_ = true;
  ev_loop(EV_DEFAULT_UC_ 0);
}

void
Promise::Detach (void)
{
  if (blocking_) {
    ev_unloop(EV_DEFAULT_ EVUNLOOP_ONE);
    blocking_ = false;
  }
  if (ref_) {
    ev_unref(EV_DEFAULT_UC);
  }
  ObjectWrap::Detach();
}

Promise*
Promise::Create (bool ref)
{
  HandleScope scope;

  Local<Object> handle =
    Promise::constructor_template->GetFunction()->NewInstance();

  Promise *promise = ObjectWrap::Unwrap<Promise>(handle);

  promise->ref_ = ref;
  if (ref) ev_ref(EV_DEFAULT_UC);

  return promise;
}

bool
Promise::EmitSuccess (int argc, v8::Handle<v8::Value> argv[])
{
  bool r = Emit("success", argc, argv);

  Detach();

  return r;
}

bool
Promise::EmitError (int argc, v8::Handle<v8::Value> argv[])
{
  bool r = Emit("error", argc, argv);

  Detach();

  return r;
}
