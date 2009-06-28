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
  Local<Value> emit_v = handle_->Get(String::NewSymbol("emit"));
  assert(emit_v->IsFunction());
  Local<Function> emit = Local<Function>::Cast(emit_v);

  Local<Array> event_args = Array::New(argc);
  for (int i = 0; i < argc; i++) {
    event_args->Set(Integer::New(i), argv[i]);
  }

  Handle<Value> emit_argv[2] = { String::NewSymbol(type), event_args };

  TryCatch try_catch;

  emit->Call(handle_, 2, emit_argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
    return false;
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

  Promise *promise = new Promise(handle);
  ObjectWrap::InformV8ofAllocation(promise);

  promise->Attach();
  ev_unref(EV_DEFAULT_UC);

  return promise;
}

bool
Promise::EmitSuccess (int argc, v8::Handle<v8::Value> argv[])
{
  bool r = Emit("Success", argc, argv);

  Detach();
  ev_ref(EV_DEFAULT_UC);

  return r;
}

bool
Promise::EmitError (int argc, v8::Handle<v8::Value> argv[])
{
  bool r = Emit("Error", argc, argv);

  Detach();
  ev_ref(EV_DEFAULT_UC);

  return r;
}
