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


#include "node.h"
#include "node_buffer.h"

#include "v8.h"
#include "v8-profiler.h"

#include <assert.h>
#include <string.h> // memcpy

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define BUFFER_CLASS_ID (0xBABE)

namespace node {

using namespace v8;

#define SLICE_ARGS(start_arg, end_arg)                               \
  if (!start_arg->IsInt32() || !end_arg->IsInt32()) {                \
    return ThrowException(Exception::TypeError(                      \
          String::New("Bad argument.")));                            \
  }                                                                  \
  int32_t start = start_arg->Int32Value();                           \
  int32_t end = end_arg->Int32Value();                               \
  if (start < 0 || end < 0) {                                        \
    return ThrowException(Exception::TypeError(                      \
          String::New("Bad argument.")));                            \
  }                                                                  \
  if (!(start <= end)) {                                             \
    return ThrowException(Exception::Error(                          \
          String::New("Must have start <= end")));                   \
  }                                                                  \
  if ((size_t)end > parent->length_) {                               \
    return ThrowException(Exception::Error(                          \
          String::New("end cannot be longer than parent.length")));  \
  }


static Persistent<String> length_symbol;
static Persistent<String> chars_written_sym;
static Persistent<String> write_sym;
Persistent<FunctionTemplate> Buffer::constructor_template;


static inline size_t base64_decoded_size(const char *src, size_t size) {
  const char *const end = src + size;
  const int remainder = size % 4;

  size = (size / 4) * 3;
  if (remainder) {
    if (size == 0 && remainder == 1) {
      // special case: 1-byte input cannot be decoded
      size = 0;
    } else {
      // non-padded input, add 1 or 2 extra bytes
      size += 1 + (remainder == 3);
    }
  }

  // check for trailing padding (1 or 2 bytes)
  if (size > 0) {
    if (end[-1] == '=') size--;
    if (end[-2] == '=') size--;
  }

  return size;
}


static size_t ByteLength (Handle<String> string, enum encoding enc) {
  HandleScope scope;

  if (enc == UTF8) {
    return string->Utf8Length();
  } else if (enc == BASE64) {
    String::Utf8Value v(string);
    return base64_decoded_size(*v, v.length());
  } else if (enc == UCS2) {
    return string->Length() * 2;
  } else if (enc == HEX) {
    return string->Length() / 2;
  } else {
    return string->Length();
  }
}


Handle<Object> Buffer::New(Handle<String> string) {
  HandleScope scope;

  // get Buffer from global scope.
  Local<Object> global = v8::Context::GetCurrent()->Global();
  Local<Value> bv = global->Get(String::NewSymbol("Buffer"));
  assert(bv->IsFunction());
  Local<Function> b = Local<Function>::Cast(bv);

  Local<Value> argv[1] = { Local<Value>::New(node_isolate, string) };
  Local<Object> instance = b->NewInstance(1, argv);

  return scope.Close(instance);
}


Buffer* Buffer::New(size_t length) {
  HandleScope scope;

  Local<Value> arg = Integer::NewFromUnsigned(length, node_isolate);
  Local<Object> b = constructor_template->GetFunction()->NewInstance(1, &arg);
  if (b.IsEmpty()) return NULL;

  return ObjectWrap::Unwrap<Buffer>(b);
}


Buffer* Buffer::New(const char* data, size_t length) {
  HandleScope scope;

  Local<Value> arg = Integer::NewFromUnsigned(0, node_isolate);
  Local<Object> obj = constructor_template->GetFunction()->NewInstance(1, &arg);

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(obj);
  buffer->Replace(const_cast<char*>(data), length, NULL, NULL);

  return buffer;
}


Buffer* Buffer::New(char *data, size_t length,
                    free_callback callback, void *hint) {
  HandleScope scope;

  Local<Value> arg = Integer::NewFromUnsigned(0, node_isolate);
  Local<Object> obj = constructor_template->GetFunction()->NewInstance(1, &arg);

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(obj);
  buffer->Replace(data, length, callback, hint);

  return buffer;
}


Handle<Value> Buffer::New(const Arguments &args) {
  if (!args.IsConstructCall()) {
    return FromConstructorTemplate(constructor_template, args);
  }

  HandleScope scope;

  if (!args[0]->IsUint32()) return ThrowTypeError("Bad argument");

  size_t length = args[0]->Uint32Value();
  if (length > Buffer::kMaxLength) {
    return ThrowRangeError("length > kMaxLength");
  }
  new Buffer(args.This(), length);

  return args.This();
}


Buffer::Buffer(Handle<Object> wrapper, size_t length) : ObjectWrap() {
  Wrap(wrapper);

  length_ = 0;
  callback_ = NULL;
  handle_.SetWrapperClassId(BUFFER_CLASS_ID);

  Replace(NULL, length, NULL, NULL);
}


Buffer::~Buffer() {
  Replace(NULL, 0, NULL, NULL);
}


// if replace doesn't have a callback, data must be copied
// const_cast in Buffer::New requires this
void Buffer::Replace(char *data, size_t length,
                     free_callback callback, void *hint) {
  HandleScope scope;

  if (callback_) {
    callback_(data_, callback_hint_);
  } else if (length_) {
    delete [] data_;
    V8::AdjustAmountOfExternalAllocatedMemory(
        -static_cast<intptr_t>(sizeof(Buffer) + length_));
  }

  length_ = length;
  callback_ = callback;
  callback_hint_ = hint;

  if (callback_) {
    data_ = data;
  } else if (length_) {
    data_ = new char[length_];
    if (data)
      memcpy(data_, data, length_);
    V8::AdjustAmountOfExternalAllocatedMemory(sizeof(Buffer) + length_);
  } else {
    data_ = NULL;
  }

  handle_->SetIndexedPropertiesToExternalArrayData(data_,
                                                   kExternalUnsignedByteArray,
                                                   length_);
  handle_->Set(length_symbol, Integer::NewFromUnsigned(length_, node_isolate));
}


Handle<Value> Buffer::BinarySlice(const Arguments &args) {
  HandleScope scope;
  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[0], args[1])

  char *data = parent->data_ + start;
  //Local<String> string = String::New(data, end - start);

  Local<Value> b =  Encode(data, end - start, BINARY);

  return scope.Close(b);
}


Handle<Value> Buffer::AsciiSlice(const Arguments &args) {
  HandleScope scope;
  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[0], args[1])

  char* data = parent->data_ + start;
  Local<String> string = String::New(data, end - start);

  return scope.Close(string);
}


Handle<Value> Buffer::Utf8Slice(const Arguments &args) {
  HandleScope scope;
  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[0], args[1])
  char *data = parent->data_ + start;
  Local<String> string = String::New(data, end - start);
  return scope.Close(string);
}

Handle<Value> Buffer::Ucs2Slice(const Arguments &args) {
  HandleScope scope;
  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[0], args[1])
  uint16_t *data = (uint16_t*)(parent->data_ + start);
  Local<String> string = String::New(data, (end - start) / 2);
  return scope.Close(string);
}


// supports regular and URL-safe base64
static const int unbase64_table[] =
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-2,-1,-1,-2,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,62,-1,63
  ,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1
  ,-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14
  ,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63
  ,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40
  ,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  };
#define unbase64(x) unbase64_table[(uint8_t)(x)]


Handle<Value> Buffer::Base64Slice(const Arguments &args) {
  HandleScope scope;
  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[0], args[1])

  unsigned slen = end - start;
  const char* src = parent->data_ + start;

  unsigned dlen = (slen + 2 - ((slen + 2) % 3)) / 3 * 4;
  char* dst = new char[dlen];

  unsigned a;
  unsigned b;
  unsigned c;
  unsigned i;
  unsigned k;
  unsigned n;

  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz"
                              "0123456789+/";

  i = 0;
  k = 0;
  n = slen / 3 * 3;

  while (i < n) {
    a = src[i + 0] & 0xff;
    b = src[i + 1] & 0xff;
    c = src[i + 2] & 0xff;

    dst[k + 0] = table[a >> 2];
    dst[k + 1] = table[((a & 3) << 4) | (b >> 4)];
    dst[k + 2] = table[((b & 0x0f) << 2) | (c >> 6)];
    dst[k + 3] = table[c & 0x3f];

    i += 3;
    k += 4;
  }

  if (n != slen) {
    switch (slen - n) {
    case 1:
      a = src[i + 0] & 0xff;
      dst[k + 0] = table[a >> 2];
      dst[k + 1] = table[(a & 3) << 4];
      dst[k + 2] = '=';
      dst[k + 3] = '=';
      break;

    case 2:
      a = src[i + 0] & 0xff;
      b = src[i + 1] & 0xff;
      dst[k + 0] = table[a >> 2];
      dst[k + 1] = table[((a & 3) << 4) | (b >> 4)];
      dst[k + 2] = table[(b & 0x0f) << 2];
      dst[k + 3] = '=';
      break;
    }
  }

  Local<String> string = String::New(dst, dlen);
  delete [] dst;

  return scope.Close(string);
}


// buffer.fill(value, start, end);
Handle<Value> Buffer::Fill(const Arguments &args) {
  HandleScope scope;

  if (!args[0]->IsInt32()) {
    return ThrowException(Exception::Error(String::New(
            "value is not a number")));
  }
  int value = (char)args[0]->Int32Value();

  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[1], args[2])

  memset( (void*)(parent->data_ + start),
          value,
          end - start);

  return Undefined(node_isolate);
}


// var bytesCopied = buffer.copy(target, targetStart, sourceStart, sourceEnd);
Handle<Value> Buffer::Copy(const Arguments &args) {
  HandleScope scope;

  Buffer *source = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!Buffer::HasInstance(args[0])) {
    return ThrowException(Exception::TypeError(String::New(
            "First arg should be a Buffer")));
  }

  Local<Object> target = args[0]->ToObject();
  char* target_data = Buffer::Data(target);
  size_t target_length = Buffer::Length(target);
  size_t target_start = args[1]->Uint32Value();
  size_t source_start = args[2]->Uint32Value();
  size_t source_end = args[3]->IsUint32() ? args[3]->Uint32Value()
                                          : source->length_;

  if (source_end < source_start) {
    return ThrowException(Exception::Error(String::New(
            "sourceEnd < sourceStart")));
  }

  // Copy 0 bytes; we're done
  if (source_end == source_start) {
    return scope.Close(Integer::New(0, node_isolate));
  }

  if (target_start >= target_length) {
    return ThrowException(Exception::Error(String::New(
            "targetStart out of bounds")));
  }

  if (source_start >= source->length_) {
    return ThrowException(Exception::Error(String::New(
            "sourceStart out of bounds")));
  }

  if (source_end > source->length_) {
    return ThrowException(Exception::Error(String::New(
            "sourceEnd out of bounds")));
  }

  size_t to_copy = MIN(MIN(source_end - source_start,
                           target_length - target_start),
                           source->length_ - source_start);

  // need to use slightly slower memmove is the ranges might overlap
  memmove((void *)(target_data + target_start),
          (const void*)(source->data_ + source_start),
          to_copy);

  return scope.Close(Integer::New(to_copy, node_isolate));
}


// var charsWritten = buffer.utf8Write(string, offset, [maxLength]);
Handle<Value> Buffer::Utf8Write(const Arguments &args) {
  HandleScope scope;
  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();

  size_t offset = args[1]->Uint32Value();

  int length = s->Length();

  if (length == 0) {
    constructor_template->GetFunction()->Set(chars_written_sym,
                                             Integer::New(0, node_isolate));
    return scope.Close(Integer::New(0, node_isolate));
  }

  if (length > 0 && offset >= buffer->length_) {
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  size_t max_length = args[2]->IsUndefined() ? buffer->length_ - offset
                                             : args[2]->Uint32Value();
  max_length = MIN(buffer->length_ - offset, max_length);

  char* p = buffer->data_ + offset;

  int char_written;

  int written = s->WriteUtf8(p,
                             max_length,
                             &char_written,
                             (String::HINT_MANY_WRITES_EXPECTED |
                              String::NO_NULL_TERMINATION));

  constructor_template->GetFunction()->Set(chars_written_sym,
                                           Integer::New(char_written,
                                                        node_isolate));

  return scope.Close(Integer::New(written, node_isolate));
}


// var charsWritten = buffer.ucs2Write(string, offset, [maxLength]);
Handle<Value> Buffer::Ucs2Write(const Arguments &args) {
  HandleScope scope;
  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();

  size_t offset = args[1]->Uint32Value();

  if (s->Length() > 0 && offset >= buffer->length_) {
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  size_t max_length = args[2]->IsUndefined() ? buffer->length_ - offset
                                             : args[2]->Uint32Value();
  max_length = MIN(buffer->length_ - offset, max_length) / 2;

  uint16_t* p = (uint16_t*)(buffer->data_ + offset);

  int written = s->Write(p,
                         0,
                         max_length,
                         (String::HINT_MANY_WRITES_EXPECTED |
                          String::NO_NULL_TERMINATION));

  constructor_template->GetFunction()->Set(chars_written_sym,
                                           Integer::New(written, node_isolate));

  return scope.Close(Integer::New(written * 2, node_isolate));
}


// var charsWritten = buffer.asciiWrite(string, offset);
Handle<Value> Buffer::AsciiWrite(const Arguments &args) {
  HandleScope scope;

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();
  size_t length = s->Length();
  size_t offset = args[1]->Int32Value();

  if (length > 0 && offset >= buffer->length_) {
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  size_t max_length = args[2]->IsUndefined() ? buffer->length_ - offset
                                             : args[2]->Uint32Value();
  max_length = MIN(length, MIN(buffer->length_ - offset, max_length));

  char *p = buffer->data_ + offset;

  int written = s->WriteAscii(p,
                              0,
                              max_length,
                              (String::HINT_MANY_WRITES_EXPECTED |
                               String::NO_NULL_TERMINATION));

  constructor_template->GetFunction()->Set(chars_written_sym,
                                           Integer::New(written, node_isolate));

  return scope.Close(Integer::New(written, node_isolate));
}


// var bytesWritten = buffer.base64Write(string, offset, [maxLength]);
Handle<Value> Buffer::Base64Write(const Arguments &args) {
  HandleScope scope;

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  String::AsciiValue s(args[0]);
  size_t length = s.length();
  size_t offset = args[1]->Int32Value();
  size_t max_length = args[2]->IsUndefined() ? buffer->length_ - offset
                                             : args[2]->Uint32Value();
  max_length = MIN(length, MIN(buffer->length_ - offset, max_length));

  if (max_length && offset >= buffer->length_) {
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  char a, b, c, d;
  char* start = buffer->data_ + offset;
  char* dst = start;
  char* const dstEnd = dst + max_length;
  const char* src = *s;
  const char* const srcEnd = src + s.length();

  while (src < srcEnd && dst < dstEnd) {
    int remaining = srcEnd - src;

    while (unbase64(*src) < 0 && src < srcEnd) src++, remaining--;
    if (remaining == 0 || *src == '=') break;
    a = unbase64(*src++);

    while (unbase64(*src) < 0 && src < srcEnd) src++, remaining--;
    if (remaining <= 1 || *src == '=') break;
    b = unbase64(*src++);

    *dst++ = (a << 2) | ((b & 0x30) >> 4);
    if (dst == dstEnd) break;

    while (unbase64(*src) < 0 && src < srcEnd) src++, remaining--;
    if (remaining <= 2 || *src == '=') break;
    c = unbase64(*src++);

    *dst++ = ((b & 0x0F) << 4) | ((c & 0x3C) >> 2);
    if (dst == dstEnd) break;

    while (unbase64(*src) < 0 && src < srcEnd) src++, remaining--;
    if (remaining <= 3 || *src == '=') break;
    d = unbase64(*src++);

    *dst++ = ((c & 0x03) << 6) | (d & 0x3F);
  }

  constructor_template->GetFunction()->Set(chars_written_sym,
                                           Integer::New(dst - start,
                                                        node_isolate));

  return scope.Close(Integer::New(dst - start, node_isolate));
}


Handle<Value> Buffer::BinaryWrite(const Arguments &args) {
  HandleScope scope;

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();
  size_t length = s->Length();
  size_t offset = args[1]->Int32Value();

  if (s->Length() > 0 && offset >= buffer->length_) {
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  char *p = (char*)buffer->data_ + offset;

  size_t max_length = args[2]->IsUndefined() ? buffer->length_ - offset
                                             : args[2]->Uint32Value();
  max_length = MIN(length, MIN(buffer->length_ - offset, max_length));

  int written = DecodeWrite(p, max_length, s, BINARY);

  constructor_template->GetFunction()->Set(chars_written_sym,
                                           Integer::New(written, node_isolate));

  return scope.Close(Integer::New(written, node_isolate));
}


// var nbytes = Buffer.byteLength("string", "utf8")
Handle<Value> Buffer::ByteLength(const Arguments &args) {
  HandleScope scope;

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();
  enum encoding e = ParseEncoding(args[1], UTF8);

  return scope.Close(Integer::New(node::ByteLength(s, e), node_isolate));
}


Handle<Value> Buffer::MakeFastBuffer(const Arguments &args) {
  HandleScope scope;

  if (!Buffer::HasInstance(args[0])) {
    return ThrowException(Exception::TypeError(String::New(
            "First argument must be a Buffer")));
  }

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());
  Local<Object> fast_buffer = args[1]->ToObject();;
  uint32_t offset = args[2]->Uint32Value();
  uint32_t length = args[3]->Uint32Value();

  fast_buffer->SetIndexedPropertiesToExternalArrayData(buffer->data_ + offset,
                                                       kExternalUnsignedByteArray,
                                                       length);

  return Undefined(node_isolate);
}


bool Buffer::HasInstance(v8::Handle<v8::Value> val) {
  if (!val->IsObject()) return false;
  v8::Local<v8::Object> obj = val->ToObject();

  if (obj->GetIndexedPropertiesExternalArrayDataType() == kExternalUnsignedByteArray)
    return true;

  // Also check for SlowBuffers that are empty.
  if (constructor_template->HasInstance(obj))
    return true;

  return false;
}


class RetainedBufferInfo: public v8::RetainedObjectInfo {
public:
  RetainedBufferInfo(Buffer* buffer);
  virtual void Dispose();
  virtual bool IsEquivalent(RetainedObjectInfo* other);
  virtual intptr_t GetHash();
  virtual const char* GetLabel();
  virtual intptr_t GetSizeInBytes();
private:
  Buffer* buffer_;
  static const char label[];
};

const char RetainedBufferInfo::label[] = "Buffer";


RetainedBufferInfo::RetainedBufferInfo(Buffer* buffer): buffer_(buffer) {
}


void RetainedBufferInfo::Dispose() {
  buffer_ = NULL;
  delete this;
}


bool RetainedBufferInfo::IsEquivalent(RetainedObjectInfo* other) {
  return label == other->GetLabel() &&
         buffer_ == static_cast<RetainedBufferInfo*>(other)->buffer_;
}


intptr_t RetainedBufferInfo::GetHash() {
  return reinterpret_cast<intptr_t>(buffer_);
}


const char* RetainedBufferInfo::GetLabel() {
  return label;
}


intptr_t RetainedBufferInfo::GetSizeInBytes() {
  return Buffer::Length(buffer_);
}


RetainedObjectInfo* WrapperInfo(uint16_t class_id, Handle<Value> wrapper) {
  assert(class_id == BUFFER_CLASS_ID);
  assert(Buffer::HasInstance(wrapper));
  Buffer* buffer = Buffer::Unwrap<Buffer>(wrapper.As<Object>());
  return new RetainedBufferInfo(buffer);
}


void Buffer::Initialize(Handle<Object> target) {
  HandleScope scope;

  // sanity checks
  assert(unbase64('/') == 63);
  assert(unbase64('+') == 62);
  assert(unbase64('T') == 19);
  assert(unbase64('Z') == 25);
  assert(unbase64('t') == 45);
  assert(unbase64('z') == 51);
  assert(unbase64(' ') == -2);
  assert(unbase64('\n') == -2);
  assert(unbase64('\r') == -2);

  length_symbol = NODE_PSYMBOL("length");
  chars_written_sym = NODE_PSYMBOL("_charsWritten");

  Local<FunctionTemplate> t = FunctionTemplate::New(Buffer::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("SlowBuffer"));

  // copy free
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "binarySlice", Buffer::BinarySlice);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "asciiSlice", Buffer::AsciiSlice);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "base64Slice", Buffer::Base64Slice);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "ucs2Slice", Buffer::Ucs2Slice);
  // TODO NODE_SET_PROTOTYPE_METHOD(t, "utf16Slice", Utf16Slice);
  // copy
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "utf8Slice", Buffer::Utf8Slice);

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "utf8Write", Buffer::Utf8Write);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "asciiWrite", Buffer::AsciiWrite);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "binaryWrite", Buffer::BinaryWrite);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "base64Write", Buffer::Base64Write);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "ucs2Write", Buffer::Ucs2Write);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "fill", Buffer::Fill);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "copy", Buffer::Copy);

  NODE_SET_METHOD(constructor_template->GetFunction(),
                  "byteLength",
                  Buffer::ByteLength);
  NODE_SET_METHOD(constructor_template->GetFunction(),
                  "makeFastBuffer",
                  Buffer::MakeFastBuffer);

  target->Set(String::NewSymbol("SlowBuffer"), constructor_template->GetFunction());

  HeapProfiler::DefineWrapperClass(BUFFER_CLASS_ID, WrapperInfo);
}


}  // namespace node

NODE_MODULE(node_buffer, node::Buffer::Initialize)
