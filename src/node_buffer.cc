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
  do {                                                                      \
    if (!(r)) return env->ThrowRangeError("out of range index");            \
  } while (0)

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
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Isolate;
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
  return type == v8::kExternalUint8Array;
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


Local<Object> New(Isolate* isolate, Handle<String> string, enum encoding enc) {
  EscapableHandleScope scope(isolate);

  size_t length = StringBytes::Size(isolate, string, enc);

  Local<Object> buf = New(length);
  char* data = Buffer::Data(buf);
  StringBytes::Write(isolate, data, length, string, enc);

  return scope.Escape(buf);
}


Local<Object> New(Isolate* isolate, size_t length) {
  EscapableHandleScope handle_scope(isolate);
  Local<Object> obj = Buffer::New(Environment::GetCurrent(isolate), length);
  return handle_scope.Escape(obj);
}


// TODO(trevnorris): these have a flaw by needing to call the Buffer inst then
// Alloc. continue to look for a better architecture.
Local<Object> New(Environment* env, size_t length) {
  EscapableHandleScope scope(env->isolate());

  assert(length <= kMaxLength);

  Local<Value> arg = Uint32::NewFromUnsigned(env->isolate(), length);
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
  smalloc::Alloc(env, obj, data, length);

  return scope.Escape(obj);
}


Local<Object> New(Isolate* isolate, const char* data, size_t length) {
  Environment* env = Environment::GetCurrent(isolate);
  EscapableHandleScope handle_scope(env->isolate());
  Local<Object> obj = Buffer::New(env, data, length);
  return handle_scope.Escape(obj);
}


// TODO(trevnorris): for backwards compatibility this is left to copy the data,
// but for consistency w/ the other should use data. And a copy version renamed
// to something else.
Local<Object> New(Environment* env, const char* data, size_t length) {
  EscapableHandleScope scope(env->isolate());

  assert(length <= kMaxLength);

  Local<Value> arg = Uint32::NewFromUnsigned(env->isolate(), length);
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

  smalloc::Alloc(env, obj, new_data, length);

  return scope.Escape(obj);
}


Local<Object> New(Isolate* isolate,
                  char* data,
                  size_t length,
                  smalloc::FreeCallback callback,
                  void* hint) {
  Environment* env = Environment::GetCurrent(isolate);
  EscapableHandleScope handle_scope(env->isolate());
  Local<Object> obj = Buffer::New(env, data, length, callback, hint);
  return handle_scope.Escape(obj);
}


Local<Object> New(Environment* env,
                  char* data,
                  size_t length,
                  smalloc::FreeCallback callback,
                  void* hint) {
  EscapableHandleScope scope(env->isolate());

  assert(length <= kMaxLength);

  Local<Value> arg = Uint32::NewFromUnsigned(env->isolate(), length);
  Local<Object> obj = env->buffer_constructor_function()->NewInstance(1, &arg);

  smalloc::Alloc(env, obj, data, length, callback, hint);

  return scope.Escape(obj);
}


Local<Object> Use(Isolate* isolate, char* data, uint32_t length) {
  Environment* env = Environment::GetCurrent(isolate);
  EscapableHandleScope handle_scope(env->isolate());
  Local<Object> obj = Buffer::Use(env, data, length);
  return handle_scope.Escape(obj);
}


Local<Object> Use(Environment* env, char* data, uint32_t length) {
  EscapableHandleScope scope(env->isolate());

  assert(length <= kMaxLength);

  Local<Value> arg = Uint32::NewFromUnsigned(env->isolate(), length);
  Local<Object> obj = env->buffer_constructor_function()->NewInstance(1, &arg);

  smalloc::Alloc(env, obj, data, length);

  return scope.Escape(obj);
}


template <encoding encoding>
void StringSlice(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  ARGS_THIS(args.This())
  SLICE_START_END(args[0], args[1], obj_length)

  args.GetReturnValue().Set(
      StringBytes::Encode(env->isolate(), obj_data + start, length, encoding));
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
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  Local<Object> target = args[0]->ToObject();

  if (!HasInstance(target))
    return env->ThrowTypeError("first arg should be a Buffer");

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
    return env->ThrowRangeError("out of range index");

  if (source_end - source_start > target_length - target_start)
    source_end = source_start + target_length - target_start;

  uint32_t to_copy = MIN(MIN(source_end - source_start,
                             target_length - target_start),
                             obj_length - source_start);

  memmove(target_data + target_start, obj_data + source_start, to_copy);
  args.GetReturnValue().Set(to_copy);
}


void Fill(const FunctionCallbackInfo<Value>& args) {
  ARGS_THIS(args[0].As<Object>())

  size_t start = args[2]->Uint32Value();
  size_t end = args[3]->Uint32Value();
  size_t length = end - start;
  CHECK(length + start <= obj_length);

  if (args[1]->IsNumber()) {
    int value = args[1]->Uint32Value() & 255;
    memset(obj_data + start, value, length);
    return;
  }

  node::Utf8Value str(args[1]);
  size_t str_length = str.length();
  size_t in_there = str_length;
  char* ptr = obj_data + start + str_length;

  if (str_length == 0)
    return;

  memcpy(obj_data + start, *str, MIN(str_length, length));

  if (str_length >= length)
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
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  ARGS_THIS(args.This())

  if (!args[0]->IsString())
    return env->ThrowTypeError("Argument must be a string");

  Local<String> str = args[0]->ToString();

  if (encoding == HEX && str->Length() % 2 != 0)
    return env->ThrowTypeError("Invalid hex string");

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
    return env->ThrowRangeError("Offset is out of bounds");

  uint32_t written = StringBytes::Write(env->isolate(),
                                        obj_data + offset,
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
  ARGS_THIS(args[0].As<Object>());

  uint32_t offset = args[1]->Uint32Value();
  CHECK_LE(offset + sizeof(T), obj_length);

  union NoAlias {
    T val;
    char bytes[sizeof(T)];
  };

  union NoAlias na;
  const char* ptr = static_cast<const char*>(obj_data) + offset;
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
  ARGS_THIS(args[0].As<Object>())

  T val = args[1]->NumberValue();
  uint32_t offset = args[2]->Uint32Value();
  CHECK_LE(offset + sizeof(T), obj_length);

  union NoAlias {
    T val;
    char bytes[sizeof(T)];
  };

  union NoAlias na = { val };
  char* ptr = static_cast<char*>(obj_data) + offset;
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


void ByteLength(const FunctionCallbackInfo<Value> &args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (!args[0]->IsString())
    return env->ThrowTypeError("Argument must be a string");

  Local<String> s = args[0]->ToString();
  enum encoding e = ParseEncoding(env->isolate(), args[1], UTF8);

  uint32_t size = StringBytes::Size(env->isolate(), s, e);
  args.GetReturnValue().Set(size);
}


void Compare(const FunctionCallbackInfo<Value> &args) {
  Local<Object> obj_a = args[0].As<Object>();
  char* obj_a_data =
      static_cast<char*>(obj_a->GetIndexedPropertiesExternalArrayData());
  size_t obj_a_len = obj_a->GetIndexedPropertiesExternalArrayDataLength();

  Local<Object> obj_b = args[1].As<Object>();
  char* obj_b_data =
      static_cast<char*>(obj_b->GetIndexedPropertiesExternalArrayData());
  size_t obj_b_len = obj_b->GetIndexedPropertiesExternalArrayDataLength();

  size_t cmp_length = MIN(obj_a_len, obj_b_len);

  int32_t val = memcmp(obj_a_data, obj_b_data, cmp_length);

  // Normalize val to be an integer in the range of [1, -1] since
  // implementations of memcmp() can vary by platform.
  if (val == 0) {
    if (obj_a_len > obj_b_len)
      val = 1;
    else if (obj_a_len < obj_b_len)
      val = -1;
  } else {
    if (val > 0)
      val = 1;
    else
      val = -1;
  }

  args.GetReturnValue().Set(val);
}


// pass Buffer object to load prototype methods
void SetupBufferJS(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  assert(args[0]->IsFunction());

  Local<Function> bv = args[0].As<Function>();
  env->set_buffer_constructor_function(bv);
  Local<Value> proto_v = bv->Get(env->prototype_string());

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

  NODE_SET_METHOD(proto, "copy", Copy);

  // for backwards compatibility
  proto->ForceSet(env->offset_string(),
                  Uint32::New(env->isolate(), 0),
                  v8::ReadOnly);

  assert(args[1]->IsObject());

  Local<Object> internal = args[1].As<Object>();
  ASSERT(internal->IsObject());

  NODE_SET_METHOD(internal, "byteLength", ByteLength);
  NODE_SET_METHOD(internal, "compare", Compare);
  NODE_SET_METHOD(internal, "fill", Fill);

  NODE_SET_METHOD(internal, "readDoubleBE", ReadDoubleBE);
  NODE_SET_METHOD(internal, "readDoubleLE", ReadDoubleLE);
  NODE_SET_METHOD(internal, "readFloatBE", ReadFloatBE);
  NODE_SET_METHOD(internal, "readFloatLE", ReadFloatLE);

  NODE_SET_METHOD(internal, "writeDoubleBE", WriteDoubleBE);
  NODE_SET_METHOD(internal, "writeDoubleLE", WriteDoubleLE);
  NODE_SET_METHOD(internal, "writeFloatBE", WriteFloatBE);
  NODE_SET_METHOD(internal, "writeFloatLE", WriteFloatLE);
}


void Initialize(Handle<Object> target,
                Handle<Value> unused,
                Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "setupBufferJS"),
              FunctionTemplate::New(env->isolate(), SetupBufferJS)
                  ->GetFunction());
}


}  // namespace Buffer
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(buffer, node::Buffer::Initialize)
