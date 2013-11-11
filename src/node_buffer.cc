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

#include "env.h"
#include "env-inl.h"
#include "smalloc.h"
#include "string_bytes.h"
#include "v8-profiler.h"
#include "v8.h"

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
    obj->GetIndexedPropertiesExternalArrayData());                          \
  if (obj_length > 0)                                                       \
    assert(obj_data != NULL);

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

using v8::ArrayBuffer;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;


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


Local<Object> New(size_t length) {
  HandleScope handle_scope(node_isolate);
  Environment* env = Environment::GetCurrent(node_isolate);
  Local<Object> obj = Buffer::New(env, length);
  return handle_scope.Close(obj);
}


// TODO(trevnorris): these have a flaw by needing to call the Buffer inst then
// Alloc. continue to look for a better architecture.
Local<Object> New(Environment* env, size_t length) {
  HandleScope scope(node_isolate);

  assert(length <= kMaxLength);

  Local<Value> arg = Uint32::NewFromUnsigned(length, node_isolate);
  Local<Object> obj = env->buffer_constructor_function()->NewInstance(1, &arg);

  // TODO(trevnorris): done like this to handle HasInstance since only checks
  // if external array data has been set, but would like to use a better
  // approach if v8 provided one.
  char* data;
  if (length > 0) {
    data = static_cast<char*>(malloc(length));
    if (data == NULL)
      FatalError("node::Buffer::New(size_t)", "Out Of Memory");
  } else {
    data = NULL;
  }
  smalloc::Alloc(obj, data, length);

  return scope.Close(obj);
}


Local<Object> New(const char* data, size_t length) {
  HandleScope handle_scope(node_isolate);
  Environment* env = Environment::GetCurrent(node_isolate);
  Local<Object> obj = Buffer::New(env, data, length);
  return handle_scope.Close(obj);
}


// TODO(trevnorris): for backwards compatibility this is left to copy the data,
// but for consistency w/ the other should use data. And a copy version renamed
// to something else.
Local<Object> New(Environment* env, const char* data, size_t length) {
  HandleScope scope(node_isolate);

  assert(length <= kMaxLength);

  Local<Value> arg = Uint32::NewFromUnsigned(length, node_isolate);
  Local<Object> obj = env->buffer_constructor_function()->NewInstance(1, &arg);

  // TODO(trevnorris): done like this to handle HasInstance since only checks
  // if external array data has been set, but would like to use a better
  // approach if v8 provided one.
  char* new_data;
  if (length > 0) {
    new_data = static_cast<char*>(malloc(length));
    if (new_data == NULL)
      FatalError("node::Buffer::New(const char*, size_t)", "Out Of Memory");
    memcpy(new_data, data, length);
  } else {
    new_data = NULL;
  }

  smalloc::Alloc(obj, new_data, length);

  return scope.Close(obj);
}


Local<Object> New(char* data,
                  size_t length,
                  smalloc::FreeCallback callback,
                  void* hint) {
  HandleScope handle_scope(node_isolate);
  Environment* env = Environment::GetCurrent(node_isolate);
  Local<Object> obj = Buffer::New(env, data, length, callback, hint);
  return handle_scope.Close(obj);
}


Local<Object> New(Environment* env,
                  char* data,
                  size_t length,
                  smalloc::FreeCallback callback,
                  void* hint) {
  HandleScope scope(node_isolate);

  assert(length <= kMaxLength);

  Local<Value> arg = Uint32::NewFromUnsigned(length, node_isolate);
  Local<Object> obj = env->buffer_constructor_function()->NewInstance(1, &arg);

  smalloc::Alloc(obj, data, length, callback, hint);

  return scope.Close(obj);
}


Local<Object> Use(char* data, uint32_t length) {
  HandleScope handle_scope(node_isolate);
  Environment* env = Environment::GetCurrent(node_isolate);
  Local<Object> obj = Buffer::Use(env, data, length);
  return handle_scope.Close(obj);
}


Local<Object> Use(Environment* env, char* data, uint32_t length) {
  HandleScope scope(node_isolate);

  assert(length <= kMaxLength);

  Local<Value> arg = Uint32::NewFromUnsigned(length, node_isolate);
  Local<Object> obj = env->buffer_constructor_function()->NewInstance(1, &arg);

  smalloc::Alloc(obj, data, length);

  return scope.Close(obj);
}


template <encoding encoding>
void StringSlice(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  ARGS_THIS(args.This())
  SLICE_START_END(args[0], args[1], obj_length)

  args.GetReturnValue().Set(
      StringBytes::Encode(obj_data + start, length, encoding));
}


void BinarySlice(const FunctionCallbackInfo<Value>& args) {
  StringSlice<BINARY>(args);
}


void AsciiSlice(const FunctionCallbackInfo<Value>& args) {
  StringSlice<ASCII>(args);
}


void Utf8Slice(const FunctionCallbackInfo<Value>& args) {
  StringSlice<UTF8>(args);
}


void Ucs2Slice(const FunctionCallbackInfo<Value>& args) {
  StringSlice<UCS2>(args);
}


void HexSlice(const FunctionCallbackInfo<Value>& args) {
  StringSlice<HEX>(args);
}


void Base64Slice(const FunctionCallbackInfo<Value>& args) {
  StringSlice<BASE64>(args);
}


// bytesCopied = buffer.copy(target[, targetStart][, sourceStart][, sourceEnd]);
void Copy(const FunctionCallbackInfo<Value> &args) {
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
    return args.GetReturnValue().Set(0);

  if (source_start > obj_length)
    return ThrowRangeError("out of range index");

  if (source_end - source_start > target_length - target_start)
    source_end = source_start + target_length - target_start;

  uint32_t to_copy = MIN(MIN(source_end - source_start,
                             target_length - target_start),
                             obj_length - source_start);

  memmove(target_data + target_start, obj_data + source_start, to_copy);
  args.GetReturnValue().Set(to_copy);
}


// buffer.fill(value[, start][, end]);
void Fill(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  ARGS_THIS(args.This())
  SLICE_START_END(args[1], args[2], obj_length)
  args.GetReturnValue().Set(args.This());

  if (args[0]->IsNumber()) {
    int value = args[0]->Uint32Value() & 255;
    memset(obj_data + start, value, length);
    return;
  }

  String::Utf8Value at(args[0]);
  size_t at_length = at.length();

  // optimize single ascii character case
  if (at_length == 1) {
    int value = static_cast<int>((*at)[0]);
    memset(obj_data + start, value, length);
    return;
  }

  size_t in_there = at_length;
  char* ptr = obj_data + start + at_length;

  memcpy(obj_data + start, *at, MIN(at_length, length));

  if (at_length >= length)
    return;

  while (in_there < length - in_there) {
    memcpy(ptr, obj_data + start, in_there);
    ptr += in_there;
    in_there *= 2;
  }

  if (in_there < length) {
    memcpy(ptr, obj_data + start, length - in_there);
    in_there = length;
  }
}


template <encoding encoding>
void StringWrite(const FunctionCallbackInfo<Value>& args) {
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
    return args.GetReturnValue().Set(0);

  if (encoding == UCS2)
    max_length = max_length / 2;

  if (offset >= obj_length)
    return ThrowRangeError("Offset is out of bounds");

  uint32_t written = StringBytes::Write(obj_data + offset,
                                        max_length,
                                        str,
                                        encoding,
                                        NULL);
  args.GetReturnValue().Set(written);
}


void Base64Write(const FunctionCallbackInfo<Value>& args) {
  StringWrite<BASE64>(args);
}


void BinaryWrite(const FunctionCallbackInfo<Value>& args) {
  StringWrite<BINARY>(args);
}


void Utf8Write(const FunctionCallbackInfo<Value>& args) {
  StringWrite<UTF8>(args);
}


void Ucs2Write(const FunctionCallbackInfo<Value>& args) {
  StringWrite<UCS2>(args);
}


void HexWrite(const FunctionCallbackInfo<Value>& args) {
  StringWrite<HEX>(args);
}


void AsciiWrite(const FunctionCallbackInfo<Value>& args) {
  StringWrite<ASCII>(args);
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
void ReadFloatGeneric(const FunctionCallbackInfo<Value>& args) {
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
  if (endianness != GetEndianness())
    Swizzle(na.bytes, sizeof(na.bytes));

  args.GetReturnValue().Set(na.val);
}


void ReadFloatLE(const FunctionCallbackInfo<Value>& args) {
  ReadFloatGeneric<float, kLittleEndian>(args);
}


void ReadFloatBE(const FunctionCallbackInfo<Value>& args) {
  ReadFloatGeneric<float, kBigEndian>(args);
}


void ReadDoubleLE(const FunctionCallbackInfo<Value>& args) {
  ReadFloatGeneric<double, kLittleEndian>(args);
}


void ReadDoubleBE(const FunctionCallbackInfo<Value>& args) {
  ReadFloatGeneric<double, kBigEndian>(args);
}


template <typename T, enum Endianness endianness>
uint32_t WriteFloatGeneric(const FunctionCallbackInfo<Value>& args) {
  bool doAssert = !args[2]->BooleanValue();

  T val = static_cast<T>(args[0]->NumberValue());
  size_t offset;

  if (!ParseArrayIndex(args[1], 0, &offset)) {
    ThrowRangeError("out of range index");
    return 0;
  }

  if (doAssert) {
    size_t len = Length(args.This());
    if (offset + sizeof(T) > len || offset + sizeof(T) < offset) {
      ThrowRangeError("Trying to write beyond buffer length");
      return 0;
    }
  }

  union NoAlias {
    T val;
    char bytes[sizeof(T)];
  };

  union NoAlias na = { val };
  void* data = args.This()->GetIndexedPropertiesExternalArrayData();
  char* ptr = static_cast<char*>(data) + offset;
  if (endianness != GetEndianness())
    Swizzle(na.bytes, sizeof(na.bytes));
  memcpy(ptr, na.bytes, sizeof(na.bytes));
  return offset + sizeof(na.bytes);
}


void WriteFloatLE(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(WriteFloatGeneric<float, kLittleEndian>(args));
}


void WriteFloatBE(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(WriteFloatGeneric<float, kBigEndian>(args));
}


void WriteDoubleLE(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(WriteFloatGeneric<double, kLittleEndian>(args));
}


void WriteDoubleBE(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(WriteFloatGeneric<double, kBigEndian>(args));
}


void ToArrayBuffer(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  ARGS_THIS(args.This());
  void* adata = malloc(obj_length);

  if (adata == NULL) {
    FatalError("node::Buffer::ToArrayBuffer("
        "const FunctionCallbackInfo<v8::Value>&)",
        "Out Of Memory");
  }

  memcpy(adata, obj_data, obj_length);

  Local<ArrayBuffer> abuf = ArrayBuffer::New(adata, obj_length);
  args.GetReturnValue().Set(abuf);
}


void ByteLength(const FunctionCallbackInfo<Value> &args) {
  HandleScope scope(node_isolate);

  if (!args[0]->IsString())
    return ThrowTypeError("Argument must be a string");

  Local<String> s = args[0]->ToString();
  enum encoding e = ParseEncoding(args[1], UTF8);

  uint32_t size = StringBytes::Size(s, e);
  args.GetReturnValue().Set(size);
}


// pass Buffer object to load prototype methods
void SetupBufferJS(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());

  assert(args[0]->IsFunction());

  Local<Function> bv = args[0].As<Function>();
  env->set_buffer_constructor_function(bv);
  Local<Value> proto_v =
      bv->Get(FIXED_ONE_BYTE_STRING(node_isolate, "prototype"));

  assert(proto_v->IsObject());

  Local<Object> proto = proto_v.As<Object>();

  NODE_SET_METHOD(proto, "asciiSlice", AsciiSlice);
  NODE_SET_METHOD(proto, "base64Slice", Base64Slice);
  NODE_SET_METHOD(proto, "binarySlice", BinarySlice);
  NODE_SET_METHOD(proto, "hexSlice", HexSlice);
  NODE_SET_METHOD(proto, "ucs2Slice", Ucs2Slice);
  NODE_SET_METHOD(proto, "utf8Slice", Utf8Slice);

  NODE_SET_METHOD(proto, "asciiWrite", AsciiWrite);
  NODE_SET_METHOD(proto, "base64Write", Base64Write);
  NODE_SET_METHOD(proto, "binaryWrite", BinaryWrite);
  NODE_SET_METHOD(proto, "hexWrite", HexWrite);
  NODE_SET_METHOD(proto, "ucs2Write", Ucs2Write);
  NODE_SET_METHOD(proto, "utf8Write", Utf8Write);

  NODE_SET_METHOD(proto, "readDoubleBE", ReadDoubleBE);
  NODE_SET_METHOD(proto, "readDoubleLE", ReadDoubleLE);
  NODE_SET_METHOD(proto, "readFloatBE", ReadFloatBE);
  NODE_SET_METHOD(proto, "readFloatLE", ReadFloatLE);

  NODE_SET_METHOD(proto, "writeDoubleBE", WriteDoubleBE);
  NODE_SET_METHOD(proto, "writeDoubleLE", WriteDoubleLE);
  NODE_SET_METHOD(proto, "writeFloatBE", WriteFloatBE);
  NODE_SET_METHOD(proto, "writeFloatLE", WriteFloatLE);

  NODE_SET_METHOD(proto, "toArrayBuffer", ToArrayBuffer);

  NODE_SET_METHOD(proto, "copy", Copy);
  NODE_SET_METHOD(proto, "fill", Fill);

  // for backwards compatibility
  proto->Set(FIXED_ONE_BYTE_STRING(node_isolate, "offset"),
             Uint32::New(0, node_isolate),
             v8::ReadOnly);

  assert(args[1]->IsObject());

  Local<Object> internal = args[1].As<Object>();

  internal->Set(FIXED_ONE_BYTE_STRING(node_isolate, "byteLength"),
                FunctionTemplate::New(ByteLength)->GetFunction());
}


void Initialize(Handle<Object> target,
                Handle<Value> unused,
                Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "setupBufferJS"),
              FunctionTemplate::New(SetupBufferJS)->GetFunction());
}


}  // namespace Buffer
}  // namespace node

NODE_MODULE_CONTEXT_AWARE(node_buffer, node::Buffer::Initialize)
