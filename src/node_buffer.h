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

#ifndef NODE_BUFFER_H_
#define NODE_BUFFER_H_

#include "node.h"
#include "node_object_wrap.h"
#include "v8.h"
#include <assert.h>

namespace node {

/* A buffer is a chunk of memory stored outside the V8 heap, mirrored by an
 * object in javascript. The object is not totally opaque, one can access
 * individual bytes with [] and slice it into substrings or sub-buffers
 * without copying memory.
 */

/*
   The C++ API for Buffer changed radically between v0.2 and v0.3, in fact
   it was the reason for bumping the version. In v0.2 JavaScript Buffers and
   C++ Buffers were in one-to-one correspondence via ObjectWrap. We found
   that it was faster to expose the C++ Buffers to JavaScript as a
   "SlowBuffer" which is used as a private backend to pure JavaScript
   "Buffer" objects - a 'Buffer' in v0.3 might look like this:

   { _parent: s,
     _offset: 520,
     length: 5 }

   Migrating code C++ Buffer code from v0.2 to v0.3 is difficult. Here are
   some tips:
    - buffer->data() calls should become Buffer::Data(buffer) calls.
    - buffer->length() calls should become Buffer::Length(buffer) calls.
    - There should not be any ObjectWrap::Unwrap<Buffer>() calls. You should
      not be storing pointers to Buffer objects at all - as they are
      now considered internal structures. Instead consider making a
      JavaScript reference to the buffer.

   See the source code node-png as an example of a module which successfully
   compiles on both v0.2 and v0.3 while making heavy use of the C++ Buffer
   API.

 */


class NODE_EXTERN Buffer: public ObjectWrap {
 public:
  // mirrors deps/v8/src/objects.h
  static const unsigned int kMaxLength = 0x3fffffff;

  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  static bool HasInstance(v8::Handle<v8::Value> val);

  static inline char* Data(v8::Handle<v8::Value> val) {
    assert(val->IsObject());
    void* data = val.As<v8::Object>()->GetIndexedPropertiesExternalArrayData();
    return static_cast<char*>(data);
  }

  static inline char* Data(Buffer *b) {
    return Buffer::Data(b->handle_);
  }

  static inline size_t Length(v8::Handle<v8::Value> val) {
    assert(val->IsObject());
    int len = val.As<v8::Object>()
              ->GetIndexedPropertiesExternalArrayDataLength();
    return static_cast<size_t>(len);
  }

  static inline size_t Length(Buffer *b) {
    return Buffer::Length(b->handle_);
  }


  ~Buffer();

  typedef void (*free_callback)(char *data, void *hint);

  // C++ API for constructing fast buffer
  static v8::Handle<v8::Object> New(v8::Handle<v8::String> string);

  static void Initialize(v8::Handle<v8::Object> target);

  // public constructor
  static Buffer* New(size_t length);
  // public constructor - data is copied
  static Buffer* New(const char *data, size_t len);
  // public constructor
  static Buffer* New(char *data, size_t length,
                     free_callback callback, void *hint);

  private:
  static v8::Handle<v8::Value> New(const v8::Arguments &args);

  template <encoding encoding>
  static v8::Handle<v8::Value> StringSlice(const v8::Arguments &args);
  static v8::Handle<v8::Value> BinarySlice(const v8::Arguments &args);
  static v8::Handle<v8::Value> AsciiSlice(const v8::Arguments &args);
  static v8::Handle<v8::Value> Base64Slice(const v8::Arguments &args);
  static v8::Handle<v8::Value> Utf8Slice(const v8::Arguments &args);
  static v8::Handle<v8::Value> Ucs2Slice(const v8::Arguments &args);
  static v8::Handle<v8::Value> HexSlice(const v8::Arguments &args);

  template <encoding encoding>
  static v8::Handle<v8::Value> StringWrite(const v8::Arguments &args);
  static v8::Handle<v8::Value> BinaryWrite(const v8::Arguments &args);
  static v8::Handle<v8::Value> Base64Write(const v8::Arguments &args);
  static v8::Handle<v8::Value> AsciiWrite(const v8::Arguments &args);
  static v8::Handle<v8::Value> Utf8Write(const v8::Arguments &args);
  static v8::Handle<v8::Value> Ucs2Write(const v8::Arguments &args);
  static v8::Handle<v8::Value> HexWrite(const v8::Arguments &args);
  static v8::Handle<v8::Value> ReadFloatLE(const v8::Arguments &args);
  static v8::Handle<v8::Value> ReadFloatBE(const v8::Arguments &args);
  static v8::Handle<v8::Value> ReadDoubleLE(const v8::Arguments &args);
  static v8::Handle<v8::Value> ReadDoubleBE(const v8::Arguments &args);
  static v8::Handle<v8::Value> WriteFloatLE(const v8::Arguments &args);
  static v8::Handle<v8::Value> WriteFloatBE(const v8::Arguments &args);
  static v8::Handle<v8::Value> WriteDoubleLE(const v8::Arguments &args);
  static v8::Handle<v8::Value> WriteDoubleBE(const v8::Arguments &args);
  static v8::Handle<v8::Value> ByteLength(const v8::Arguments &args);
  static v8::Handle<v8::Value> MakeFastBuffer(const v8::Arguments &args);
  static v8::Handle<v8::Value> Fill(const v8::Arguments &args);
  static v8::Handle<v8::Value> Copy(const v8::Arguments &args);

  Buffer(v8::Handle<v8::Object> wrapper, size_t length);
  void Replace(char *data, size_t length, free_callback callback, void *hint);

  size_t length_;
  char* data_;
  free_callback callback_;
  void* callback_hint_;
};


}  // namespace node buffer

#endif  // NODE_BUFFER_H_
