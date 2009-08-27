#ifndef node_h
#define node_h

#include <ev.h>
#include <eio.h>
#include <v8.h>
#include <evcom.h>

#include "object_wrap.h"
#include "node_version.h"

namespace node {

#define NODE_DEFINE_CONSTANT(target, constant)                            \
  (target)->Set(v8::String::NewSymbol(#constant),                         \
                v8::Integer::New(constant))

#define NODE_SET_METHOD(obj, name, callback)                              \
  obj->Set(v8::String::NewSymbol(name),                                   \
           v8::FunctionTemplate::New(callback)->GetFunction())

#define NODE_SET_PROTOTYPE_METHOD(templ, name, callback)                  \
do {                                                                      \
  v8::Local<v8::Signature> __callback##_SIG = v8::Signature::New(templ);              \
  v8::Local<v8::FunctionTemplate> __callback##_TEM =                              \
    FunctionTemplate::New(callback, v8::Handle<v8::Value>() , __callback##_SIG ); \
  templ->PrototypeTemplate()->Set(v8::String::NewSymbol(name),            \
                                  __callback##_TEM);                      \
} while(0)

enum encoding {ASCII, UTF8, RAW};
enum encoding ParseEncoding (v8::Handle<v8::Value> encoding_v);
void FatalException (v8::TryCatch &try_catch);
int start (int argc, char *argv[]);

} // namespace node
#endif // node_h
