#ifndef node_h
#define node_h

#include <ev.h>
#include <eio.h>
#include <v8.h>
#include <evcom.h>

#include "object_wrap.h"
#include "node_version.h"

namespace node {

/* Converts a unixtime to V8 Date */
#define NODE_UNIXTIME_V8(t) v8::Date::New(1000*static_cast<double>(t))
#define NODE_V8_UNIXTIME(v) ((double)((v)->IntegerValue())/1000.0);

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

enum encoding {ASCII, UTF8, RAW, RAWS};
enum encoding ParseEncoding (v8::Handle<v8::Value> encoding_v, enum encoding _default = RAW);
void FatalException (v8::TryCatch &try_catch);

v8::Local<v8::Value>
Encode (const void *buf, size_t len, enum encoding encoding = RAW);

// Returns -1 if the handle was not valid for decoding 
ssize_t
DecodeBytes (v8::Handle<v8::Value>, enum encoding encoding = RAW);

// returns bytes written.
ssize_t
DecodeWrite (char *buf, size_t buflen, v8::Handle<v8::Value>, enum encoding encoding = RAW);


} // namespace node
#endif // node_h
