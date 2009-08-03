#ifndef node_h
#define node_h

#include <ev.h>
#include <eio.h>
#include <v8.h>
#include <evcom.h>

#include "object_wrap.h"

namespace node {

#define NODE_VERSION "0.1.2"

#define NODE_DEFINE_CONSTANT(target, constant)                            \
  (target)->Set(v8::String::NewSymbol(#constant),                         \
                v8::Integer::New(constant))

#define NODE_SET_METHOD(obj, name, callback)                              \
  obj->Set(v8::String::NewSymbol(name),                                   \
           v8::FunctionTemplate::New(callback)->GetFunction())

#define NODE_SET_PROTOTYPE_METHOD(templ, name, callback)                  \
do {                                                                      \
  Local<Signature> __callback##_SIG = Signature::New(templ);              \
  Local<FunctionTemplate> __callback##_TEM =                              \
    FunctionTemplate::New(callback, Handle<Value>() , __callback##_SIG ); \
  templ->PrototypeTemplate()->Set(v8::String::NewSymbol(name),            \
                                  __callback##_TEM);                      \
} while(0)

enum encoding {ASCII, UTF8, RAW};
enum encoding ParseEncoding (v8::Handle<v8::Value> encoding_v);
void FatalException (v8::TryCatch &try_catch); 
evcom_buf * buf_new (size_t size);

} // namespace node
#endif // node_h
