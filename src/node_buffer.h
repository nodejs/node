// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
NODE_EXTERN v8::Local<v8::Object> New(size_t length);
// public constructor from string
NODE_EXTERN v8::Local<v8::Object> New(v8::Handle<v8::String> string,
                                      enum encoding enc = UTF8);
// public constructor - data is copied
// TODO(trevnorris): should be something like Copy()
NODE_EXTERN v8::Local<v8::Object> New(const char* data, size_t len);
// public constructor - data is used, callback is passed data on object gc
NODE_EXTERN v8::Local<v8::Object> New(char* data,
                                      size_t length,
                                      smalloc::FreeCallback callback,
                                      void* hint);

// public constructor - data is used.
// TODO(trevnorris): should be New() for consistency
NODE_EXTERN v8::Local<v8::Object> Use(char* data, uint32_t len);

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
