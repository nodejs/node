#ifndef SRC_NODE_BUFFER_H_
#define SRC_NODE_BUFFER_H_

#include "node.h"
#include "smalloc.h"
#include "v8.h"

#if defined(NODE_WANT_INTERNALS)
#include "env.h"
#endif  // defined(NODE_WANT_INTERNALS)

namespace node {
namespace Buffer {

static const unsigned int kMaxLength = smalloc::kMaxLength;

NODE_EXTERN bool HasInstance(v8::Handle<v8::Value> val);
NODE_EXTERN bool HasInstance(v8::Handle<v8::Object> val);
NODE_EXTERN char* Data(v8::Handle<v8::Value> val);
NODE_EXTERN char* Data(v8::Handle<v8::Object> val);
NODE_EXTERN size_t Length(v8::Handle<v8::Value> val);
NODE_EXTERN size_t Length(v8::Handle<v8::Object> val);

// public constructor
NODE_EXTERN v8::Local<v8::Object> New(v8::Isolate* isolate, size_t length);
NODE_DEPRECATED("Use New(isolate, ...)",
                inline v8::Local<v8::Object> New(size_t length) {
  return New(v8::Isolate::GetCurrent(), length);
})
// public constructor from string
NODE_EXTERN v8::Local<v8::Object> New(v8::Isolate* isolate,
                                      v8::Handle<v8::String> string,
                                      enum encoding enc = UTF8);
NODE_DEPRECATED("Use New(isolate, ...)",
                inline v8::Local<v8::Object> New(v8::Handle<v8::String> string,
                                                 enum encoding enc = UTF8) {
  return New(v8::Isolate::GetCurrent(), string, enc);
})
// public constructor - data is copied
// TODO(trevnorris): should be something like Copy()
NODE_EXTERN v8::Local<v8::Object> New(v8::Isolate* isolate,
                                      const char* data,
                                      size_t len);
NODE_DEPRECATED("Use New(isolate, ...)",
                inline v8::Local<v8::Object> New(const char* data, size_t len) {
  return New(v8::Isolate::GetCurrent(), data, len);
})
// public constructor - data is used, callback is passed data on object gc
NODE_EXTERN v8::Local<v8::Object> New(v8::Isolate* isolate,
                                      char* data,
                                      size_t length,
                                      smalloc::FreeCallback callback,
                                      void* hint);
NODE_DEPRECATED("Use New(isolate, ...)",
                inline v8::Local<v8::Object> New(char* data,
                                                 size_t length,
                                                 smalloc::FreeCallback callback,
                                                 void* hint) {
  return New(v8::Isolate::GetCurrent(), data, length, callback, hint);
})

// public constructor - data is used.
// TODO(trevnorris): should be New() for consistency
NODE_EXTERN v8::Local<v8::Object> Use(v8::Isolate* isolate,
                                      char* data,
                                      uint32_t len);
NODE_DEPRECATED("Use Use(isolate, ...)",
                inline v8::Local<v8::Object> Use(char* data, uint32_t len) {
  return Use(v8::Isolate::GetCurrent(), data, len);
})

// This is verbose to be explicit with inline commenting
static inline bool IsWithinBounds(size_t off, size_t len, size_t max) {
  // Asking to seek too far into the buffer
  // check to avoid wrapping in subsequent subtraction
  if (off > max)
    return false;

  // Asking for more than is left over in the buffer
  if (max - off < len)
    return false;

  // Otherwise we're in bounds
  return true;
}

// Internal. Not for public consumption. We can't define these in
// src/node_internals.h due to a circular dependency issue with
// the smalloc.h and node_internals.h headers.
#if defined(NODE_WANT_INTERNALS)
v8::Local<v8::Object> New(Environment* env, size_t size);
v8::Local<v8::Object> New(Environment* env, const char* data, size_t len);
v8::Local<v8::Object> New(Environment* env,
                          char* data,
                          size_t length,
                          smalloc::FreeCallback callback,
                          void* hint);
v8::Local<v8::Object> Use(Environment* env, char* data, uint32_t length);
#endif  // defined(NODE_WANT_INTERNALS)

}  // namespace Buffer
}  // namespace node

#endif  // SRC_NODE_BUFFER_H_
