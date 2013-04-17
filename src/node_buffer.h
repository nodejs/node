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

#include "smalloc.h"
#include "v8.h"

#ifndef NODE_BUFFER_H_
#define NODE_BUFFER_H_

namespace node {

namespace Buffer {

static const unsigned int kMaxLength = smalloc::kMaxLength;

bool HasInstance(v8::Handle<v8::Value> val);
bool HasInstance(v8::Handle<v8::Object> val);
char* Data(v8::Handle<v8::Value> val);
char* Data(v8::Handle<v8::Object> val);
size_t Length(v8::Handle<v8::Value> val);
size_t Length(v8::Handle<v8::Object> val);

// public constructor
v8::Local<v8::Object> New(size_t length);
// public constructor from string
v8::Local<v8::Object> New(v8::Handle<v8::String> string);
// public constructor - data is copied
// TODO(trevnorris): should be something like Copy()
v8::Local<v8::Object> New(const char* data, size_t len);
// public constructor - data is used, callback is passed data on object gc
v8::Local<v8::Object> New(char* data,
                          size_t length,
                          smalloc::FreeCallback callback,
                          void* hint);

// public constructor - data is used.
// TODO(trevnorris): should be New() for consistency
v8::Local<v8::Object> Use(char* data, uint32_t len);

}  // namespace Buffer

}  // namespace node

#endif  // NODE_BUFFER_H_
