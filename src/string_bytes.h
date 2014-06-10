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

#ifndef SRC_STRING_BYTES_H_
#define SRC_STRING_BYTES_H_

// Decodes a v8::Handle<v8::String> or Buffer to a raw char*

#include "v8.h"
#include "node.h"

namespace node {

extern int WRITE_UTF8_FLAGS;

class StringBytes {
 public:
  // Does the string match the encoding? Quick but non-exhaustive.
  // Example: a HEX string must have a length that's a multiple of two.
  // FIXME(bnoordhuis) IsMaybeValidString()? Naming things is hard...
  static bool IsValidString(v8::Isolate* isolate,
                            v8::Handle<v8::String> string,
                            enum encoding enc);

  // Fast, but can be 2 bytes oversized for Base64, and
  // as much as triple UTF-8 strings <= 65536 chars in length
  static size_t StorageSize(v8::Isolate* isolate,
                            v8::Handle<v8::Value> val,
                            enum encoding enc);

  // Precise byte count, but slightly slower for Base64 and
  // very much slower for UTF-8
  static size_t Size(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     enum encoding enc);

  // If the string is external then assign external properties to data and len,
  // then return true. If not return false.
  static bool GetExternalParts(v8::Isolate* isolate,
                               v8::Handle<v8::Value> val,
                               const char** data,
                               size_t* len);

  // Write the bytes from the string or buffer into the char*
  // returns the number of bytes written, which will always be
  // <= buflen.  Use StorageSize/Size first to know how much
  // memory to allocate.
  static size_t Write(v8::Isolate* isolate,
                      char* buf,
                      size_t buflen,
                      v8::Handle<v8::Value> val,
                      enum encoding enc,
                      int* chars_written = NULL);

  // Take the bytes in the src, and turn it into a Buffer or String.
  static v8::Local<v8::Value> Encode(v8::Isolate* isolate,
                                     const char* buf,
                                     size_t buflen,
                                     enum encoding encoding);

  // Deprecated legacy interface

  NODE_DEPRECATED("Use IsValidString(isolate, ...)",
                  static inline bool IsValidString(
      v8::Handle<v8::String> string,
      enum encoding enc) {
    return IsValidString(v8::Isolate::GetCurrent(), string, enc);
  })

  NODE_DEPRECATED("Use StorageSize(isolate, ...)",
                  static inline size_t StorageSize(v8::Handle<v8::Value> val,
                                                  enum encoding enc) {
    return StorageSize(v8::Isolate::GetCurrent(), val, enc);
  })

  NODE_DEPRECATED("Use Size(isolate, ...)",
                  static inline size_t Size(v8::Handle<v8::Value> val,
                                            enum encoding enc) {
    return Size(v8::Isolate::GetCurrent(), val, enc);
  })

  NODE_DEPRECATED("Use GetExternalParts(isolate, ...)",
                  static inline bool GetExternalParts(v8::Handle<v8::Value> val,
                                                      const char** data,
                                                      size_t* len) {
    return GetExternalParts(v8::Isolate::GetCurrent(), val, data, len);
  })

  NODE_DEPRECATED("Use Write(isolate, ...)",
                  static inline size_t Write(char* buf,
                                             size_t buflen,
                                             v8::Handle<v8::Value> val,
                                             enum encoding enc,
                                             int* chars_written = NULL) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    return Write(isolate, buf, buflen, val, enc, chars_written);
  })

  NODE_DEPRECATED("Use Encode(isolate, ...)",
                  static inline v8::Local<v8::Value> Encode(
      const char* buf,
      size_t buflen,
      enum encoding encoding) {
    return Encode(v8::Isolate::GetCurrent(), buf, buflen, encoding);
  })
};

}  // namespace node

#endif  // SRC_STRING_BYTES_H_
