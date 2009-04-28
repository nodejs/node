#ifndef node_h
#define node_h

#include <ev.h>
#include <eio.h>
#include <v8.h>

#define NODE_SYMBOL(name) v8::String::NewSymbol(name)
#define NODE_METHOD(name) v8::Handle<v8::Value> name (const v8::Arguments& args)
#define NODE_SET_METHOD(obj, name, callback) \
  obj->Set(NODE_SYMBOL(name), v8::FunctionTemplate::New(callback)->GetFunction())

enum encoding {UTF8, RAW};

void node_fatal_exception (v8::TryCatch &try_catch); 
void node_exit (int code);

// call this after creating a new eio event.
void node_eio_warmup (void);

#endif // node_h

