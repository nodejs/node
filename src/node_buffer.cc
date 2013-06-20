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
#include "node_internals.h"
#include "node_buffer.h"
#include "smalloc.h"
#include "string_bytes.h"

#include "v8.h"
#include "v8-profiler.h"

#include <assert.h>
#include <string.h>
#include <limits.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define CHECK_NOT_OOB(r)                                                    \
  do { if (!(r)) return ThrowRangeError("out of range index"); } while (0)

#define ARGS_THIS(argT)                                                     \
  Local<Object> obj = argT;                                                 \
  size_t obj_length = obj->GetIndexedPropertiesExternalArrayDataLength();   \
  char* obj_data = static_cast<char*>(                                      \
    obj->GetIndexedPropertiesExternalArrayData());

#define SLICE_START_END(start_arg, end_arg, end_max)                        \
  size_t start;                                                             \
  size_t end;                                                               \
  CHECK_NOT_OOB(ParseArrayIndex(start_arg, 0, &start));                     \
  CHECK_NOT_OOB(ParseArrayIndex(end_arg, end_max, &end));                   \
  if (end < start) end = start;                                             \
  CHECK_NOT_OOB(end <= end_max);                                            \
  size_t length = end - start;

namespace node {

namespace Buffer {


using v8::Arguments;
using v8::Function;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::Uint32;
using v8::Undefined;
using v8::Value;


Persistent<Function> p_buffer_fn;


bool HasInstance(Handle<Value> val) {
  return val->IsObject() && HasInstance(val.As<Object>());
}


bool HasInstance(Handle<Object> obj) {
  if (!obj->HasIndexedPropertiesInExternalArrayData())
    return false;
  v8::ExternalArrayType type = obj->GetIndexedPropertiesExternalArrayDataType();
  return type == v8::kExternalUnsignedByteArray;
}


char* Data(Handle<Value> val) {
  assert(val->IsObject());
  // Use a fully qualified name here to work around a bug in gcc 4.2.
  // It mistakes an unadorned call to Data() for the v8::String::Data type.
  return node::Buffer::Data(val.As<Object>());
}


char* Data(Handle<Object> obj) {
  assert(obj->HasIndexedPropertiesInExternalArrayData());
  return static_cast<char*>(obj->GetIndexedPropertiesExternalArrayData());
}


size_t Length(Handle<Value> val) {
  assert(val->IsObject());
  return Length(val.As<Object>());
}


size_t Length(Handle<Object> obj) {
  assert(obj->HasIndexedPropertiesInExternalArrayData());
  return obj->GetIndexedPropertiesExternalArrayDataLength();
}


Local<Object> New(Handle<String> string, enum encoding enc) {
  HandleScope scope(node_isolate);

  size_t length = StringBytes::Size(string, enc);

  Local<Object> buf = New(length);
  char* data = Buffer::Data(buf);
  StringBytes::Write(data, length, string, enc);

  return scope.Close(buf);
}


// TODO(trevnorris): these have a flaw by needing to call the Buffer inst then
// Alloc. continue to look for a better architecture.
Local<Object> New(size_t length) {
  HandleScope scope(node_isolate);

  assert(length <= kMaxLength);

  Handle<Value> argv[2];
  // this is safe b/c Undefined and length fits in an SMI, so there's no risk
  // of GC reclaiming the values prematurely.
  argv[0] = Undefined(node_isolate);
  argv[1] = Uint32::New(length, node_isolate);
  Local<Object> obj = p_buffer_fn->NewInstance(2, argv);

  smalloc::Alloc(obj, new char[length], length);

  return scope.Close(obj);
}


// TODO(trevnorris): for backwards compatibility this is left to copy the data,
// but for consistency w/ the other should use data. And a copy version renamed
// to something else.
Local<Object> New(const char* data, size_t length) {
  HandleScope scope(node_isolate);

  assert(length <= kMaxLength);

  Handle<Value> argv[2];
  // this is safe b/c Undefined and length fits in an SMI, so there's no risk
  // of GC reclaiming the values prematurely.
  argv[0] = Undefined(node_isolate);
  argv[1] = Uint32::New(length, node_isolate);
  Local<Object> obj = p_buffer_fn->NewInstance(2, argv);

  char* new_data = new char[length];
  memcpy(new_data, data, length);

  smalloc::Alloc(obj, new_data, length);

  return scope.Close(obj);
}


Local<Object> New(char* data,
                  size_t length,
                  smalloc::FreeCallback callback,
                  void* hint) {
  HandleScope scope(node_isolate);

  assert(length <= kMaxLength);

  Handle<Value> argv[2];
  // this is safe b/c Undefined and length fits in an SMI, so there's no risk
  // of GC reclaiming the values prematurely.
  argv[0] = Undefined(node_isolate);
  argv[1] = Uint32::New(length, node_isolate);
  Local<Object> obj = p_buffer_fn->NewInstance(2, argv);

  smalloc::Alloc(obj, data, length, callback, hint);

  return scope.Close(obj);
}


Local<Object> Use(char* data, uint32_t length) {
  HandleScope scope(node_isolate);

  assert(length <= kMaxLength);

  Handle<Value> argv[2];
  // this is safe b/c Undefined and length fits in an SMI, so there's no risk
  // of GC reclaiming the values prematurely.
  argv[0] = Undefined(node_isolate);
  argv[1] = Uint32::New(length, node_isolate);
  Local<Object> obj = p_buffer_fn->NewInstance(2, argv);

  smalloc::Alloc(obj, data, length);

  return scope.Close(obj);
}


template <encoding encoding>
Handle<Value> StringSlice(const Arguments& args) {
  HandleScope scope(node_isolate);

  ARGS_THIS(args.This())
  SLICE_START_END(args[0], args[1], obj_length)

  return scope.Close(StringBytes::Encode(obj_data + start, length, encoding));
}


Handle<Value> BinarySlice(const Arguments& args) {
  return StringSlice<BINARY>(args);
}


Handle<Value> AsciiSlice(const Arguments& args) {
  return StringSlice<ASCII>(args);
}


Handle<Value> Utf8Slice(const Arguments& args) {
  return StringSlice<UTF8>(args);
}


Handle<Value> Ucs2Slice(const Arguments& args) {
  return StringSlice<UCS2>(args);
}



Handle<Value> HexSlice(const Arguments& args) {
  return StringSlice<HEX>(args);
}


Handle<Value> Base64Slice(const Arguments& args) {
  return StringSlice<BASE64>(args);
}


// bytesCopied = buffer.copy(target[, targetStart][, sourceStart][, sourceEnd]);
Handle<Value> Copy(const Arguments &args) {
  HandleScope scope(node_isolate);

  Local<Object> target = args[0]->ToObject();

  if (!HasInstance(target))
    return ThrowTypeError("first arg should be a Buffer");

  ARGS_THIS(args.This())
  size_t target_length = target->GetIndexedPropertiesExternalArrayDataLength();
  char* target_data = static_cast<char*>(
      target->GetIndexedPropertiesExternalArrayData());
  size_t target_start;
  size_t source_start;
  size_t source_end;

  CHECK_NOT_OOB(ParseArrayIndex(args[1], 0, &target_start));
  CHECK_NOT_OOB(ParseArrayIndex(args[2], 0, &source_start));
  CHECK_NOT_OOB(ParseArrayIndex(args[3], obj_length, &source_end));

  // Copy 0 bytes; we're done
  if (target_start >= target_length || source_start >= source_end)
    return scope.Close(Uint32::New(0, node_isolate));

  if (source_start > obj_length)
    return ThrowRangeError("out of range index");

  if (source_end - source_start > target_length - target_start)
    source_end = source_start + target_length - target_start;

  size_t to_copy = MIN(MIN(source_end - source_start,
                           target_length - target_start),
                           obj_length - source_start);

  memmove(target_data + target_start, obj_data + source_start, to_copy);

  return scope.Close(Uint32::New(to_copy, node_isolate));
}


// buffer.fill(value[, start][, end]);
Handle<Value> Fill(const Arguments &args) {
  HandleScope scope(node_isolate);

  ARGS_THIS(args.This())
  SLICE_START_END(args[1], args[2], obj_length)

  if (args[0]->IsNumber()) {
    int value = args[0]->Uint32Value() & 255;
    memset(obj_data + start, value, length);
    return args.This();
  }

  String::Utf8Value at(args[0]);
  size_t at_length = at.length();

  // optimize single ascii character case
  if (at_length == 1) {
    int value = static_cast<int>((*at)[0]);
    memset(obj_data + start, value, length);
    return args.This();
  }

  size_t in_there = at_length;
  char* ptr = obj_data + start + at_length;

  memcpy(obj_data + start, *at, MIN(at_length, length));

  if (at_length >= length)
    return args.This();

  while (in_there < length - in_there) {
    memcpy(ptr, obj_data + start, in_there);
    ptr += in_there;
    in_there *= 2;
  }

  if (in_there < length) {
    memcpy(ptr, obj_data + start, length - in_there);
    in_there = length;
  }

  return args.This();
}


template <encoding encoding>
Handle<Value> StringWrite(const Arguments& args) {
  HandleScope scope(node_isolate);

  ARGS_THIS(args.This())

  if (!args[0]->IsString())
    return ThrowTypeError("Argument must be a string");

  Local<String> str = args[0]->ToString();

  if (encoding == HEX && str->Length() % 2 != 0)
    return ThrowTypeError("Invalid hex string");

  size_t offset;
  size_t max_length;

  CHECK_NOT_OOB(ParseArrayIndex(args[1], 0, &offset));
  CHECK_NOT_OOB(ParseArrayIndex(args[2], obj_length - offset, &max_length));

  max_length = MIN(obj_length - offset, max_length);

  if (max_length == 0)
    return scope.Close(Uint32::New(0, node_isolate));

  if (encoding == UCS2)
    max_length = max_length / 2;

  if (offset >= obj_length)
    return ThrowRangeError("Offset is out of bounds");

  size_t written = StringBytes::Write(obj_data + offset,
                                      max_length,
                                      str,
                                      encoding,
                                      NULL);

  return scope.Close(Uint32::New(written, node_isolate));
}


Handle<Value> Base64Write(const Arguments& args) {
  return StringWrite<BASE64>(args);
}


Handle<Value> BinaryWrite(const Arguments& args) {
  return StringWrite<BINARY>(args);
}


Handle<Value> Utf8Write(const Arguments& args) {
  return StringWrite<UTF8>(args);
}


Handle<Value> Ucs2Write(const Arguments& args) {
  return StringWrite<UCS2>(args);
}


Handle<Value> HexWrite(const Arguments& args) {
  return StringWrite<HEX>(args);
}


Handle<Value> AsciiWrite(const Arguments& args) {
  return StringWrite<ASCII>(args);
}


static inline void Swizzle(char* start, unsigned int len) {
  char* end = start + len - 1;
  while (start < end) {
    char tmp = *start;
    *start++ = *end;
    *end-- = tmp;
  }
}


template <typename T, enum Endianness endianness>
Handle<Value> ReadFloatGeneric(const Arguments& args) {
  bool doAssert = !args[1]->BooleanValue();
  size_t offset;

  CHECK_NOT_OOB(ParseArrayIndex(args[0], 0, &offset));

  if (doAssert) {
    size_t len = Length(args.This());
    if (offset + sizeof(T) > len || offset + sizeof(T) < offset)
      return ThrowRangeError("Trying to read beyond buffer length");
  }

  union NoAlias {
    T val;
    char bytes[sizeof(T)];
  };

  union NoAlias na;
  const void* data = args.This()->GetIndexedPropertiesExternalArrayData();
  const char* ptr = static_cast<const char*>(data) + offset;
  memcpy(na.bytes, ptr, sizeof(na.bytes));
  if (endianness != GetEndianness()) Swizzle(na.bytes, sizeof(na.bytes));

  return Number::New(na.val);
}


Handle<Value> ReadFloatLE(const Arguments& args) {
  return ReadFloatGeneric<float, kLittleEndian>(args);
}


Handle<Value> ReadFloatBE(const Arguments& args) {
  return ReadFloatGeneric<float, kBigEndian>(args);
}


Handle<Value> ReadDoubleLE(const Arguments& args) {
  return ReadFloatGeneric<double, kLittleEndian>(args);
}


Handle<Value> ReadDoubleBE(const Arguments& args) {
  return ReadFloatGeneric<double, kBigEndian>(args);
}


template <typename T, enum Endianness endianness>
Handle<Value> WriteFloatGeneric(const Arguments& args) {
  bool doAssert = !args[2]->BooleanValue();

  T val = static_cast<T>(args[0]->NumberValue());
  size_t offset;

  CHECK_NOT_OOB(ParseArrayIndex(args[1], 0, &offset));

  if (doAssert) {
    size_t len = Length(args.This());
    if (offset + sizeof(T) > len || offset + sizeof(T) < offset)
      return ThrowRangeError("Trying to write beyond buffer length");
  }

  union NoAlias {
    T val;
    char bytes[sizeof(T)];
  };

  union NoAlias na = { val };
  void* data = args.This()->GetIndexedPropertiesExternalArrayData();
  char* ptr = static_cast<char*>(data) + offset;
  if (endianness != GetEndianness()) Swizzle(na.bytes, sizeof(na.bytes));
  memcpy(ptr, na.bytes, sizeof(na.bytes));

  return Undefined(node_isolate);
}


Handle<Value> WriteFloatLE(const Arguments& args) {
  return WriteFloatGeneric<float, kLittleEndian>(args);
}


Handle<Value> WriteFloatBE(const Arguments& args) {
  return WriteFloatGeneric<float, kBigEndian>(args);
}


Handle<Value> WriteDoubleLE(const Arguments& args) {
  return WriteFloatGeneric<double, kLittleEndian>(args);
}


Handle<Value> WriteDoubleBE(const Arguments& args) {
  return WriteFloatGeneric<double, kBigEndian>(args);
}


Handle<Value> ByteLength(const Arguments &args) {
  HandleScope scope(node_isolate);

  if (!args[0]->IsString())
    return ThrowTypeError("Argument must be a string");

  Local<String> s = args[0]->ToString();
  enum encoding e = ParseEncoding(args[1], UTF8);

  return scope.Close(Uint32::New(StringBytes::Size(s, e), node_isolate));
}


// pass Buffer object to load prototype methods
Handle<Value> SetupBufferJS(const Arguments& args) {
  HandleScope scope(node_isolate);

  assert(args[0]->IsFunction());

  Local<Function> bv = args[0].As<Function>();
  p_buffer_fn = Persistent<Function>::New(node_isolate, bv);
  Local<Value> proto_v = bv->Get(String::New("prototype"));

  assert(proto_v->IsObject());

  Local<Object> proto = proto_v.As<Object>();

  bv->Set(String::New("byteLength"),
          FunctionTemplate::New(ByteLength)->GetFunction());

  proto->Set(String::New("asciiSlice"),
             FunctionTemplate::New(AsciiSlice)->GetFunction());
  proto->Set(String::New("base64Slice"),
             FunctionTemplate::New(Base64Slice)->GetFunction());
  proto->Set(String::New("binarySlice"),
             FunctionTemplate::New(BinarySlice)->GetFunction());
  proto->Set(String::New("hexSlice"),
             FunctionTemplate::New(HexSlice)->GetFunction());
  proto->Set(String::New("ucs2Slice"),
             FunctionTemplate::New(Ucs2Slice)->GetFunction());
  proto->Set(String::New("utf8Slice"),
             FunctionTemplate::New(Utf8Slice)->GetFunction());

  proto->Set(String::New("asciiWrite"),
             FunctionTemplate::New(AsciiWrite)->GetFunction());
  proto->Set(String::New("base64Write"),
             FunctionTemplate::New(Base64Write)->GetFunction());
  proto->Set(String::New("binaryWrite"),
             FunctionTemplate::New(BinaryWrite)->GetFunction());
  proto->Set(String::New("hexWrite"),
             FunctionTemplate::New(HexWrite)->GetFunction());
  proto->Set(String::New("ucs2Write"),
             FunctionTemplate::New(Ucs2Write)->GetFunction());
  proto->Set(String::New("utf8Write"),
             FunctionTemplate::New(Utf8Write)->GetFunction());

  proto->Set(String::New("readDoubleBE"),
             FunctionTemplate::New(ReadDoubleBE)->GetFunction());
  proto->Set(String::New("readDoubleLE"),
             FunctionTemplate::New(ReadDoubleLE)->GetFunction());
  proto->Set(String::New("readFloatBE"),
             FunctionTemplate::New(ReadFloatBE)->GetFunction());
  proto->Set(String::New("readFloatLE"),
             FunctionTemplate::New(ReadFloatLE)->GetFunction());

  proto->Set(String::New("writeDoubleBE"),
             FunctionTemplate::New(WriteDoubleBE)->GetFunction());
  proto->Set(String::New("writeDoubleLE"),
             FunctionTemplate::New(WriteDoubleLE)->GetFunction());
  proto->Set(String::New("writeFloatBE"),
             FunctionTemplate::New(WriteFloatBE)->GetFunction());
  proto->Set(String::New("writeFloatLE"),
             FunctionTemplate::New(WriteFloatLE)->GetFunction());

  proto->Set(String::New("copy"),
             FunctionTemplate::New(Copy)->GetFunction());
  proto->Set(String::New("fill"),
             FunctionTemplate::New(Fill)->GetFunction());

  // for backwards compatibility
  proto->Set(String::New("offset"), Uint32::New(0, node_isolate), v8::ReadOnly);

  return Undefined(node_isolate);
}


void Initialize(Handle<Object> target) {
  HandleScope scope(node_isolate);

  target->Set(String::New("setupBufferJS"),
              FunctionTemplate::New(SetupBufferJS)->GetFunction());
}


}  // namespace Buffer

}  // namespace node

NODE_MODULE(node_buffer, node::Buffer::Initialize)
