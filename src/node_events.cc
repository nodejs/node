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

static Persistent<String> events_symbol;

void EventEmitter::Initialize(Local<FunctionTemplate> ctemplate) {
  HandleScope scope;

  constructor_template = Persistent<FunctionTemplate>::New(ctemplate);

  Local<FunctionTemplate> __emit = FunctionTemplate::New(Emit);
  constructor_template->PrototypeTemplate()->Set(String::NewSymbol("emit"),
      __emit);
  constructor_template->SetClassName(String::NewSymbol("EventEmitter"));

  events_symbol = NODE_PSYMBOL("_events");

  // All other prototype methods are defined in events.js
}

static bool ReallyEmit(Handle<Object> self,
                       Handle<String> event,
                       int argc,
                       Handle<Value> argv[]) {
  // HandleScope not needed here because only called from one of the two
  // functions below
  Local<Value> events_v = self->Get(events_symbol);
  if (!events_v->IsObject()) return false;
  Local<Object> events = events_v->ToObject();

  Local<Value> listeners_v = events->Get(event);
  Local<Function> listener;

  TryCatch try_catch;

  if (listeners_v->IsFunction()) {
    // Optimized one-listener case
    Local<Function> listener = Local<Function>::Cast(listeners_v);

    listener->Call(self, argc, argv);

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
      return false;
    }

  } else if (listeners_v->IsArray()) {
    Local<Array> listeners = Local<Array>::Cast(listeners_v);

    for (uint32_t i = 0; i < listeners->Length(); i++) {
      Local<Value> listener_v = listeners->Get(i);
      if (!listener_v->IsFunction()) continue;
      Local<Function> listener = Local<Function>::Cast(listener_v);

      listener->Call(self, argc, argv);

      if (try_catch.HasCaught()) {
        FatalException(try_catch);
        return false;
      }
    }

  } else {
    return false;
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

bool EventEmitter::Emit(Handle<String> event, int argc, Handle<Value> argv[]) {
  HandleScope scope;
  return ReallyEmit(handle_, event, argc, argv);
}

}  // namespace node
