// Copyright 2009, Ryan Dahl <ry@tinyclouds.org>. All rights reserved.
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
#ifndef NODE_BYTE_STRING_H_
#define NODE_BYTE_STRING_H_

#include <v8.h>

namespace node {

void byte_string_init(v8::Local<v8::Object> target);

/* Byte string is an immutable chunk of data that can be created from C++
 * (byte_string_new()) or from Javascript (new ByteString(string, encoding)). It
 * provides a means of passing data through javascript, usually from one C++
 * object (like a TCP socket) to another C++ object (like an HTTP parser).
 *
 * Byte strings are always destructed by the V8 GC. Do not store references
 * to byte_string pointers, only create them and pass them into javascript,
 * or receive them as arguments out of javascript.
 */

typedef struct {
  v8::Persistent<v8::Object> handle;
  size_t length;
  char bytes[1];
} byte_string;

byte_string* byte_string_new(size_t length);

byte_string* byte_string_unwrap(v8::Handle<v8::Object> handle) {
  assert(!handle.IsEmpty());
  assert(handle->InternalFieldCount() > 0);
  return static_cast<byte_string*>(v8::Handle<v8::External>::Cast(
      handle->GetInternalField(0))->Value());
}

}  // namespace node
#endif  // NODE_BYTE_STRING_H_
