#ifndef node_h
#define node_h

#include <ev.h>
#include <eio.h>
#include <v8.h>

#define JS_SYMBOL(name) v8::String::NewSymbol(name)
#define JS_METHOD(name) v8::Handle<v8::Value> name (const v8::Arguments& args)
#define JS_SET_METHOD(obj, name, callback) \
  obj->Set(JS_SYMBOL(name), v8::FunctionTemplate::New(callback)->GetFunction())


void node_fatal_exception (v8::TryCatch &try_catch); 
#define node_loop() ev_default_loop(0)
void node_exit (int code);

// call this after creating a new eio event.
void node_eio_submit(eio_req *req);

#endif // node_h

