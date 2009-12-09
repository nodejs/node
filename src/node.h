// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef SRC_NODE_H_
#define SRC_NODE_H_

#include <ev.h>
#include <eio.h>
#include <v8.h>
#include <evcom.h>
#include <sys/types.h> /* struct stat */
#include <sys/stat.h>

#include <node_object_wrap.h>

namespace node {

#define NODE_PSYMBOL(s) Persistent<String>::New(String::NewSymbol(s))

/* Converts a unixtime to V8 Date */
#define NODE_UNIXTIME_V8(t) v8::Date::New(1000*static_cast<double>(t))
#define NODE_V8_UNIXTIME(v) (static_cast<double>((v)->IntegerValue())/1000.0);

#define NODE_DEFINE_CONSTANT(target, constant)                            \
  (target)->Set(v8::String::NewSymbol(#constant),                         \
                v8::Integer::New(constant))

#define NODE_SET_METHOD(obj, name, callback)                              \
  obj->Set(v8::String::NewSymbol(name),                                   \
           v8::FunctionTemplate::New(callback)->GetFunction())

#define NODE_SET_PROTOTYPE_METHOD(templ, name, callback)                  \
do {                                                                      \
  v8::Local<v8::Signature> __callback##_SIG = v8::Signature::New(templ);  \
  v8::Local<v8::FunctionTemplate> __callback##_TEM =                      \
    FunctionTemplate::New(callback, v8::Handle<v8::Value>(),              \
                          __callback##_SIG);                              \
  templ->PrototypeTemplate()->Set(v8::String::NewSymbol(name),            \
                                  __callback##_TEM);                      \
} while (0)

enum encoding {ASCII, UTF8, BINARY};
enum encoding ParseEncoding(v8::Handle<v8::Value> encoding_v,
                            enum encoding _default = BINARY);
void FatalException(v8::TryCatch &try_catch);

v8::Local<v8::Value> Encode(const void *buf, size_t len,
                            enum encoding encoding = BINARY);

// Returns -1 if the handle was not valid for decoding
ssize_t DecodeBytes(v8::Handle<v8::Value>,
                    enum encoding encoding = BINARY);

// returns bytes written.
ssize_t DecodeWrite(char *buf,
                    size_t buflen,
                    v8::Handle<v8::Value>,
                    enum encoding encoding = BINARY);

v8::Local<v8::Object> BuildStatsObject(struct stat * s);


}  // namespace node
#endif  // SRC_NODE_H_
