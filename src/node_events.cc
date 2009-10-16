// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#include <node_events.h>

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

#include <node.h>
#include <ev.h>
#include <v8.h>

namespace node {

using namespace v8;

Persistent<FunctionTemplate> EventEmitter::constructor_template;

/* Poor Man's coroutines */
static Promise *coroutine_top;
static int coroutine_stack_size;

void EventEmitter::Initialize(Local<FunctionTemplate> ctemplate) {
  HandleScope scope;

  constructor_template = Persistent<FunctionTemplate>::New(ctemplate);

  Local<FunctionTemplate> __emit = FunctionTemplate::New(Emit);
  constructor_template->PrototypeTemplate()->Set(String::NewSymbol("emit"),
      __emit);
  constructor_template->SetClassName(String::NewSymbol("EventEmitter"));

  // All other prototype methods are defined in events.js

  coroutine_top = NULL;
  coroutine_stack_size = 0;
}

static bool ReallyEmit(Handle<Object> self,
                       Handle<String> event,
                       int argc,
                       Handle<Value> argv[]) {
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

Handle<Value> EventEmitter::Emit(const Arguments& args) {
  HandleScope scope;

  if (args.Length() == 0) {
    return ThrowException(Exception::TypeError(
          String::New("Must give event name.")));
  }

  Local<String> event = args[0]->ToString();

  int argc = args.Length() - 1;
  Local<Value> argv[argc];

  for (int i = 0; i < argc; i++) {
    argv[i] = args[i+1];
  }

  bool r = ReallyEmit(args.Holder(), event, argc, argv);

  return scope.Close(r ? True() : False());
}

bool EventEmitter::Emit(const char *event_s, int argc, Handle<Value> argv[]) {
  HandleScope scope;
  Local<String> event = String::NewSymbol(event_s);
  return ReallyEmit(handle_, event, argc, argv);
}

Persistent<FunctionTemplate> Promise::constructor_template;

void Promise::Initialize(v8::Handle<v8::Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(EventEmitter::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Promise"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "block", Block);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "emitSuccess", EmitSuccess);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "emitError", EmitError);

  target->Set(String::NewSymbol("Promise"),
              constructor_template->GetFunction());
}

v8::Handle<v8::Value> Promise::New(const v8::Arguments& args) {
  HandleScope scope;

  Promise *promise = new Promise();
  promise->Wrap(args.This());
  promise->Attach();

  return args.This();
}

Handle<Value> Promise::Block(const Arguments& args) {
  HandleScope scope;
  Promise *promise = ObjectWrap::Unwrap<Promise>(args.Holder());
  promise->Block();
  return Undefined();
}

v8::Handle<v8::Value> Promise::EmitSuccess(const v8::Arguments& args) {
  HandleScope scope;
  Promise *promise = ObjectWrap::Unwrap<Promise>(args.Holder());

  int argc = args.Length();
  Local<Value> argv[argc];
  for (int i = 0; i < argc; i++) {
    argv[i] = args[i];
  }

  bool r = promise->EmitSuccess(argc, argv);

  return r ? True() : False();
}

v8::Handle<v8::Value> Promise::EmitError(const v8::Arguments& args) {
  HandleScope scope;
  Promise *promise = ObjectWrap::Unwrap<Promise>(args.Holder());

  int argc = args.Length();
  Local<Value> argv[argc];
  for (int i = 0; i < argc; i++) {
    argv[i] = args[i];
  }

  bool r = promise->EmitError(argc, argv);

  return r ? True() : False();
}

void Promise::Block(void) {
  blocking_ = true;

  assert(prev_ == NULL);
  if (coroutine_top) prev_ = coroutine_top;
  coroutine_top = this;
  coroutine_stack_size++;
  if (coroutine_stack_size > 10) {
    fprintf(stderr, "(node) WARNING: promise.wait() is being called too often.\n");
  }

  ev_loop(EV_DEFAULT_UC_ 0);

  assert(!blocking_);
}

void Promise::Destack() {
  assert(coroutine_top == this);
  ev_unloop(EV_DEFAULT_ EVUNLOOP_ONE);
  coroutine_top = prev_;
  prev_ = NULL;
  coroutine_stack_size--;
}

void Promise::Detach(void) {
  /* Poor Man's coroutines */
  blocking_ = false;
  while (coroutine_top && !coroutine_top->blocking_) {
    coroutine_top->Destack();
  }

  ObjectWrap::Detach();
}

bool Promise::EmitSuccess(int argc, v8::Handle<v8::Value> argv[]) {
  if (has_fired_) return false;

  bool r = Emit("success", argc, argv);

  has_fired_ = true;
  Detach();

  return r;
}

bool Promise::EmitError(int argc, v8::Handle<v8::Value> argv[]) {
  if (has_fired_) return false;

  bool r = Emit("error", argc, argv);

  has_fired_ = true;
  Detach();

  return r;
}

Promise* Promise::Create(void) {
  HandleScope scope;

  Local<Object> handle =
    Promise::constructor_template->GetFunction()->NewInstance();

  return ObjectWrap::Unwrap<Promise>(handle);
}

}  // namespace node
