#include "events.h"

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

using namespace v8;
using namespace node;

Persistent<FunctionTemplate> EventEmitter::constructor_template;

void 
EventEmitter::Initialize (v8::Handle<v8::Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New();
  constructor_template = Persistent<FunctionTemplate>::New(t);

  // All prototype methods are defined in events.js

  target->Set(String::NewSymbol("EventEmitter"),
              constructor_template->GetFunction());
}

bool
EventEmitter::Emit (const char *type, int argc, Handle<Value> argv[])
{
  HandleScope scope; 

  Local<Value> emit_v = handle_->Get(String::NewSymbol("emit"));
  assert(emit_v->IsFunction());
  Local<Function> emit = Local<Function>::Cast(emit_v);

  Local<Value> events_v = handle_->Get(String::NewSymbol("_events"));
  if (!events_v->IsObject()) return false;
  Local<Object> events = events_v->ToObject();

  Local<Value> listeners_v = events->Get(String::NewSymbol(type));
  if (!listeners_v->IsArray()) return false;
  Local<Array> listeners = Local<Array>::Cast(listeners_v);

  TryCatch try_catch;

  for (int i = 0; i < listeners->Length(); i++) {
    Local<Value> listener_v = listeners->Get(Integer::New(i));
    if (!listener_v->IsFunction()) continue;
    Local<Function> listener = Local<Function>::Cast(listener_v);

    listener->Call(handle_, argc, argv);

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
      return false;
    }
  }

  return true;
}

Persistent<FunctionTemplate> Promise::constructor_template;

void 
Promise::Initialize (v8::Handle<v8::Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New();
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(EventEmitter::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  // All prototype methods are defined in events.js

  target->Set(String::NewSymbol("Promise"),
              constructor_template->GetFunction());
}

Promise*
Promise::Create (void)
{
  HandleScope scope;

  Local<Object> handle =
    Promise::constructor_template->GetFunction()->NewInstance();

  Promise *promise = new Promise();
  promise->Wrap(handle);

  promise->Attach();
  ev_ref(EV_DEFAULT_UC);

  return promise;
}

bool
Promise::EmitSuccess (int argc, v8::Handle<v8::Value> argv[])
{
  bool r = Emit("success", argc, argv);

  Detach();
  ev_unref(EV_DEFAULT_UC);

  return r;
}

bool
Promise::EmitError (int argc, v8::Handle<v8::Value> argv[])
{
  bool r = Emit("error", argc, argv);

  Detach();
  ev_unref(EV_DEFAULT_UC);

  return r;
}
