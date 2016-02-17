#ifndef SRC_NODE_BUFFER_H_
#define SRC_NODE_BUFFER_H_

#include "node.h"
#include "v8.h"

namespace node {
namespace Buffer {

static const unsigned int kMaxLength =
    sizeof(int32_t) == sizeof(intptr_t) ? 0x3fffffff : 0x7fffffff;

NODE_EXTERN typedef void (*FreeCallback)(char* data, void* hint);

NODE_EXTERN bool HasInstance(v8::Local<v8::Value> val);
NODE_EXTERN bool HasInstance(v8::Local<v8::Object> val);
NODE_EXTERN char* Data(v8::Local<v8::Value> val);
NODE_EXTERN char* Data(v8::Local<v8::Object> val);
NODE_EXTERN size_t Length(v8::Local<v8::Value> val);
NODE_EXTERN size_t Length(v8::Local<v8::Object> val);

// public constructor - data is copied
NODE_EXTERN v8::MaybeLocal<v8::Object> Copy(v8::Isolate* isolate,
                                            const char* data,
                                            size_t len);

// public constructor
NODE_EXTERN v8::MaybeLocal<v8::Object> New(v8::Isolate* isolate, size_t length);

// public constructor from string
NODE_EXTERN v8::MaybeLocal<v8::Object> New(v8::Isolate* isolate,
                                           v8::Local<v8::String> string,
                                           enum encoding enc = UTF8);

// public constructor - data is used, callback is passed data on object gc
NODE_EXTERN v8::MaybeLocal<v8::Object> New(v8::Isolate* isolate,
                                           char* data,
                                           size_t length,
                                           FreeCallback callback,
                                           void* hint);

// public constructor - data is used.
NODE_EXTERN v8::MaybeLocal<v8::Object> New(v8::Isolate* isolate,
                                           char* data,
                                           size_t len);

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

}  // namespace Buffer
}  // namespace node

#endif  // SRC_NODE_BUFFER_H_
