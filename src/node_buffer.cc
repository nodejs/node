#include "node.h"
#include "node_buffer.h"

#include "env.h"
#include "env-inl.h"
#include "string_bytes.h"
#include "string_search.h"
#include "util.h"
#include "util-inl.h"
#include "v8-profiler.h"
#include "v8.h"

#include <string.h>
#include <limits.h>

#define BUFFER_ID 0xB0E4

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define CHECK_NOT_OOB(r)                                                    \
  do {                                                                      \
    if (!(r)) return env->ThrowRangeError("out of range index");            \
  } while (0)

#define THROW_AND_RETURN_UNLESS_BUFFER(env, obj)                            \
  do {                                                                      \
    if (!HasInstance(obj))                                                  \
      return env->ThrowTypeError("argument should be a Buffer");            \
  } while (0)

#define SPREAD_ARG(val, name)                                                 \
  CHECK((val)->IsUint8Array());                                               \
  Local<Uint8Array> name = (val).As<Uint8Array>();                            \
  ArrayBuffer::Contents name##_c = name->Buffer()->GetContents();             \
  const size_t name##_offset = name->ByteOffset();                            \
  const size_t name##_length = name->ByteLength();                            \
  char* const name##_data =                                                   \
      static_cast<char*>(name##_c.Data()) + name##_offset;                    \
  if (name##_length > 0)                                                      \
    CHECK_NE(name##_data, nullptr);

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
using v8::ArrayBufferCreationMode;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::Uint32;
using v8::Uint32Array;
using v8::Uint8Array;
using v8::Value;
using v8::WeakCallbackData;


class CallbackInfo {
 public:
  static inline void Free(char* data, void* hint);
  static inline CallbackInfo* New(Isolate* isolate,
                                  Local<Object> object,
                                  FreeCallback callback,
                                  void* hint = 0);
  inline void Dispose(Isolate* isolate);
  inline Persistent<Object>* persistent();
 private:
  static void WeakCallback(const WeakCallbackData<Object, CallbackInfo>&);
  inline void WeakCallback(Isolate* isolate, Local<Object> object);
  inline CallbackInfo(Isolate* isolate,
                      Local<Object> object,
                      FreeCallback callback,
                      void* hint);
  ~CallbackInfo();
  Persistent<Object> persistent_;
  FreeCallback const callback_;
  void* const hint_;
  DISALLOW_COPY_AND_ASSIGN(CallbackInfo);
};


void CallbackInfo::Free(char* data, void*) {
  ::free(data);
}


CallbackInfo* CallbackInfo::New(Isolate* isolate,
                                Local<Object> object,
                                FreeCallback callback,
                                void* hint) {
  return new CallbackInfo(isolate, object, callback, hint);
}


void CallbackInfo::Dispose(Isolate* isolate) {
  WeakCallback(isolate, PersistentToLocal(isolate, persistent_));
}


Persistent<Object>* CallbackInfo::persistent() {
  return &persistent_;
}


CallbackInfo::CallbackInfo(Isolate* isolate,
                           Local<Object> object,
                           FreeCallback callback,
                           void* hint)
    : persistent_(isolate, object),
      callback_(callback),
      hint_(hint) {
  persistent_.SetWeak(this, WeakCallback);
  persistent_.SetWrapperClassId(BUFFER_ID);
  persistent_.MarkIndependent();
  isolate->AdjustAmountOfExternalAllocatedMemory(sizeof(*this));
}


CallbackInfo::~CallbackInfo() {
  persistent_.Reset();
}


void CallbackInfo::WeakCallback(
    const WeakCallbackData<Object, CallbackInfo>& data) {
  data.GetParameter()->WeakCallback(data.GetIsolate(), data.GetValue());
}


void CallbackInfo::WeakCallback(Isolate* isolate, Local<Object> object) {
  CHECK(object->IsArrayBuffer());
  Local<ArrayBuffer> buf = object.As<ArrayBuffer>();
  ArrayBuffer::Contents obj_c = buf->GetContents();
  char* const obj_data = static_cast<char*>(obj_c.Data());
  if (buf->ByteLength() != 0)
    CHECK_NE(obj_data, nullptr);

  buf->Neuter();
  callback_(obj_data, hint_);
  int64_t change_in_bytes = -static_cast<int64_t>(sizeof(*this));
  isolate->AdjustAmountOfExternalAllocatedMemory(change_in_bytes);

  delete this;
}


// Buffer methods

bool HasInstance(Local<Value> val) {
  return val->IsUint8Array();
}


bool HasInstance(Local<Object> obj) {
  return obj->IsUint8Array();
}


char* Data(Local<Value> val) {
  CHECK(val->IsUint8Array());
  Local<Uint8Array> ui = val.As<Uint8Array>();
  ArrayBuffer::Contents ab_c = ui->Buffer()->GetContents();
  return static_cast<char*>(ab_c.Data()) + ui->ByteOffset();
}


char* Data(Local<Object> obj) {
  CHECK(obj->IsUint8Array());
  Local<Uint8Array> ui = obj.As<Uint8Array>();
  ArrayBuffer::Contents ab_c = ui->Buffer()->GetContents();
  return static_cast<char*>(ab_c.Data()) + ui->ByteOffset();
}


size_t Length(Local<Value> val) {
  CHECK(val->IsUint8Array());
  Local<Uint8Array> ui = val.As<Uint8Array>();
  return ui->ByteLength();
}


size_t Length(Local<Object> obj) {
  CHECK(obj->IsUint8Array());
  Local<Uint8Array> ui = obj.As<Uint8Array>();
  return ui->ByteLength();
}


MaybeLocal<Object> New(Isolate* isolate,
                       Local<String> string,
                       enum encoding enc) {
  EscapableHandleScope scope(isolate);

  size_t length = StringBytes::Size(isolate, string, enc);
  char* data = static_cast<char*>(malloc(length));

  if (data == nullptr)
    return Local<Object>();

  size_t actual = StringBytes::Write(isolate, data, length, string, enc);
  CHECK(actual <= length);

  if (actual < length) {
    data = static_cast<char*>(realloc(data, actual));
    CHECK_NE(data, nullptr);
  }

  Local<Object> buf;
  if (New(isolate, data, actual).ToLocal(&buf))
    return scope.Escape(buf);

  // Object failed to be created. Clean up resources.
  free(data);
  return Local<Object>();
}


MaybeLocal<Object> New(Isolate* isolate, size_t length) {
  EscapableHandleScope handle_scope(isolate);
  Local<Object> obj;
  if (Buffer::New(Environment::GetCurrent(isolate), length).ToLocal(&obj))
    return handle_scope.Escape(obj);
  return Local<Object>();
}


MaybeLocal<Object> New(Environment* env, size_t length) {
  EscapableHandleScope scope(env->isolate());

  // V8 currently only allows a maximum Typed Array index of max Smi.
  if (length > kMaxLength) {
    return Local<Object>();
  }

  void* data;
  if (length > 0) {
    data = malloc(length);
    if (data == nullptr)
      return Local<Object>();
  } else {
    data = nullptr;
  }

  Local<ArrayBuffer> ab =
    ArrayBuffer::New(env->isolate(),
        data,
        length,
        ArrayBufferCreationMode::kInternalized);
  Local<Uint8Array> ui = Uint8Array::New(ab, 0, length);
  Maybe<bool> mb =
      ui->SetPrototype(env->context(), env->buffer_prototype_object());
  if (mb.FromMaybe(false))
    return scope.Escape(ui);

  // Object failed to be created. Clean up resources.
  free(data);
  return Local<Object>();
}


MaybeLocal<Object> Copy(Isolate* isolate, const char* data, size_t length) {
  Environment* env = Environment::GetCurrent(isolate);
  EscapableHandleScope handle_scope(env->isolate());
  Local<Object> obj;
  if (Buffer::Copy(env, data, length).ToLocal(&obj))
    return handle_scope.Escape(obj);
  return Local<Object>();
}


MaybeLocal<Object> Copy(Environment* env, const char* data, size_t length) {
  EscapableHandleScope scope(env->isolate());

  // V8 currently only allows a maximum Typed Array index of max Smi.
  if (length > kMaxLength) {
    return Local<Object>();
  }

  void* new_data;
  if (length > 0) {
    CHECK_NE(data, nullptr);
    new_data = malloc(length);
    if (new_data == nullptr)
      return Local<Object>();
    memcpy(new_data, data, length);
  } else {
    new_data = nullptr;
  }

  Local<ArrayBuffer> ab =
    ArrayBuffer::New(env->isolate(),
        new_data,
        length,
        ArrayBufferCreationMode::kInternalized);
  Local<Uint8Array> ui = Uint8Array::New(ab, 0, length);
  Maybe<bool> mb =
      ui->SetPrototype(env->context(), env->buffer_prototype_object());
  if (mb.FromMaybe(false))
    return scope.Escape(ui);

  // Object failed to be created. Clean up resources.
  free(new_data);
  return Local<Object>();
}


MaybeLocal<Object> New(Isolate* isolate,
                       char* data,
                       size_t length,
                       FreeCallback callback,
                       void* hint) {
  Environment* env = Environment::GetCurrent(isolate);
  EscapableHandleScope handle_scope(env->isolate());
  Local<Object> obj;
  if (Buffer::New(env, data, length, callback, hint).ToLocal(&obj))
    return handle_scope.Escape(obj);
  return Local<Object>();
}


MaybeLocal<Object> New(Environment* env,
                       char* data,
                       size_t length,
                       FreeCallback callback,
                       void* hint) {
  EscapableHandleScope scope(env->isolate());

  if (length > kMaxLength) {
    return Local<Object>();
  }

  Local<ArrayBuffer> ab = ArrayBuffer::New(env->isolate(), data, length);
  Local<Uint8Array> ui = Uint8Array::New(ab, 0, length);
  Maybe<bool> mb =
      ui->SetPrototype(env->context(), env->buffer_prototype_object());

  if (!mb.FromMaybe(false))
    return Local<Object>();

  CallbackInfo::New(env->isolate(), ab, callback, hint);
  return scope.Escape(ui);
}


MaybeLocal<Object> New(Isolate* isolate, char* data, size_t length) {
  Environment* env = Environment::GetCurrent(isolate);
  EscapableHandleScope handle_scope(env->isolate());
  Local<Object> obj;
  if (Buffer::New(env, data, length).ToLocal(&obj))
    return handle_scope.Escape(obj);
  return Local<Object>();
}


MaybeLocal<Object> New(Environment* env, char* data, size_t length) {
  EscapableHandleScope scope(env->isolate());

  if (length > 0) {
    CHECK_NE(data, nullptr);
    CHECK(length <= kMaxLength);
  }

  Local<ArrayBuffer> ab =
      ArrayBuffer::New(env->isolate(),
                       data,
                       length,
                       ArrayBufferCreationMode::kInternalized);
  Local<Uint8Array> ui = Uint8Array::New(ab, 0, length);
  Maybe<bool> mb =
      ui->SetPrototype(env->context(), env->buffer_prototype_object());
  if (mb.FromMaybe(false))
    return scope.Escape(ui);
  return Local<Object>();
}


void CreateFromString(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());

  enum encoding enc = ParseEncoding(args.GetIsolate(),
                                    args[1].As<String>(),
                                    UTF8);
  Local<Object> buf;
  if (New(args.GetIsolate(), args[0].As<String>(), enc).ToLocal(&buf))
    args.GetReturnValue().Set(buf);
}


void CreateFromArrayBuffer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!args[0]->IsArrayBuffer())
    return env->ThrowTypeError("argument is not an ArrayBuffer");
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();
  Local<Uint8Array> ui = Uint8Array::New(ab, 0, ab->ByteLength());
  Maybe<bool> mb =
      ui->SetPrototype(env->context(), env->buffer_prototype_object());
  if (!mb.FromMaybe(false))
    return env->ThrowError("Unable to set Object prototype");
  args.GetReturnValue().Set(ui);
}


template <encoding encoding>
void StringSlice(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  THROW_AND_RETURN_UNLESS_BUFFER(env, args.This());
  SPREAD_ARG(args.This(), ts_obj);

  if (ts_obj_length == 0)
    return args.GetReturnValue().SetEmptyString();

  SLICE_START_END(args[0], args[1], ts_obj_length)

  args.GetReturnValue().Set(
      StringBytes::Encode(isolate, ts_obj_data + start, length, encoding));
}


template <>
void StringSlice<UCS2>(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args.This());
  SPREAD_ARG(args.This(), ts_obj);

  if (ts_obj_length == 0)
    return args.GetReturnValue().SetEmptyString();

  SLICE_START_END(args[0], args[1], ts_obj_length)
  length /= 2;

  const char* data = ts_obj_data + start;
  const uint16_t* buf;
  bool release = false;

  // Node's "ucs2" encoding expects LE character data inside a Buffer, so we
  // need to reorder on BE platforms.  See http://nodejs.org/api/buffer.html
  // regarding Node's "ucs2" encoding specification.
  const bool aligned = (reinterpret_cast<uintptr_t>(data) % sizeof(*buf) == 0);
  if (IsLittleEndian() && aligned) {
    buf = reinterpret_cast<const uint16_t*>(data);
  } else {
    // Make a copy to avoid unaligned accesses in v8::String::NewFromTwoByte().
    uint16_t* copy = new uint16_t[length];
    for (size_t i = 0, k = 0; i < length; i += 1, k += 2) {
      // Assumes that the input is little endian.
      const uint8_t lo = static_cast<uint8_t>(data[k + 0]);
      const uint8_t hi = static_cast<uint8_t>(data[k + 1]);
      copy[i] = lo | hi << 8;
    }
    buf = copy;
    release = true;
  }

  args.GetReturnValue().Set(StringBytes::Encode(env->isolate(), buf, length));

  if (release)
    delete[] buf;
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
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args.This());
  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  Local<Object> target_obj = args[0].As<Object>();
  SPREAD_ARG(args.This(), ts_obj);
  SPREAD_ARG(target_obj, target);

  size_t target_start;
  size_t source_start;
  size_t source_end;

  CHECK_NOT_OOB(ParseArrayIndex(args[1], 0, &target_start));
  CHECK_NOT_OOB(ParseArrayIndex(args[2], 0, &source_start));
  CHECK_NOT_OOB(ParseArrayIndex(args[3], ts_obj_length, &source_end));

  // Copy 0 bytes; we're done
  if (target_start >= target_length || source_start >= source_end)
    return args.GetReturnValue().Set(0);

  if (source_start > ts_obj_length)
    return env->ThrowRangeError("out of range index");

  if (source_end - source_start > target_length - target_start)
    source_end = source_start + target_length - target_start;

  uint32_t to_copy = MIN(MIN(source_end - source_start,
                             target_length - target_start),
                             ts_obj_length - source_start);

  memmove(target_data + target_start, ts_obj_data + source_start, to_copy);
  args.GetReturnValue().Set(to_copy);
}


void Fill(const FunctionCallbackInfo<Value>& args) {
  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  SPREAD_ARG(args[0], ts_obj);

  size_t start = args[2]->Uint32Value();
  size_t end = args[3]->Uint32Value();
  size_t length = end - start;
  CHECK(length + start <= ts_obj_length);

  if (args[1]->IsNumber()) {
    int value = args[1]->Uint32Value() & 255;
    memset(ts_obj_data + start, value, length);
    return;
  }

  node::Utf8Value str(args.GetIsolate(), args[1]);
  size_t str_length = str.length();
  size_t in_there = str_length;
  char* ptr = ts_obj_data + start + str_length;

  if (str_length == 0)
    return;

  memcpy(ts_obj_data + start, *str, MIN(str_length, length));

  if (str_length >= length)
    return;

  while (in_there < length - in_there) {
    memcpy(ptr, ts_obj_data + start, in_there);
    ptr += in_there;
    in_there *= 2;
  }

  if (in_there < length) {
    memcpy(ptr, ts_obj_data + start, length - in_there);
    in_there = length;
  }
}


template <encoding encoding>
void StringWrite(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args.This());
  SPREAD_ARG(args.This(), ts_obj);

  if (!args[0]->IsString())
    return env->ThrowTypeError("Argument must be a string");

  Local<String> str = args[0]->ToString(env->isolate());

  if (encoding == HEX && str->Length() % 2 != 0)
    return env->ThrowTypeError("Invalid hex string");

  size_t offset;
  size_t max_length;

  CHECK_NOT_OOB(ParseArrayIndex(args[1], 0, &offset));
  CHECK_NOT_OOB(ParseArrayIndex(args[2], ts_obj_length - offset, &max_length));

  max_length = MIN(ts_obj_length - offset, max_length);

  if (max_length == 0)
    return args.GetReturnValue().Set(0);

  if (offset >= ts_obj_length)
    return env->ThrowRangeError("Offset is out of bounds");

  uint32_t written = StringBytes::Write(env->isolate(),
                                        ts_obj_data + offset,
                                        max_length,
                                        str,
                                        encoding,
                                        nullptr);
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
  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  SPREAD_ARG(args[0], ts_obj);

  uint32_t offset = args[1]->Uint32Value();
  CHECK_LE(offset + sizeof(T), ts_obj_length);

  union NoAlias {
    T val;
    char bytes[sizeof(T)];
  };

  union NoAlias na;
  const char* ptr = static_cast<const char*>(ts_obj_data) + offset;
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
  SPREAD_ARG(args[0], ts_obj);

  T val = args[1]->NumberValue();
  uint32_t offset = args[2]->Uint32Value();
  CHECK_LE(offset + sizeof(T), ts_obj_length);

  union NoAlias {
    T val;
    char bytes[sizeof(T)];
  };

  union NoAlias na = { val };
  char* ptr = static_cast<char*>(ts_obj_data) + offset;
  if (endianness != GetEndianness())
    Swizzle(na.bytes, sizeof(na.bytes));
  memcpy(ptr, na.bytes, sizeof(na.bytes));
  return offset + sizeof(na.bytes);
}


void WriteFloatLE(const FunctionCallbackInfo<Value>& args) {
  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  args.GetReturnValue().Set(WriteFloatGeneric<float, kLittleEndian>(args));
}


void WriteFloatBE(const FunctionCallbackInfo<Value>& args) {
  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  args.GetReturnValue().Set(WriteFloatGeneric<float, kBigEndian>(args));
}


void WriteDoubleLE(const FunctionCallbackInfo<Value>& args) {
  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  args.GetReturnValue().Set(WriteFloatGeneric<double, kLittleEndian>(args));
}


void WriteDoubleBE(const FunctionCallbackInfo<Value>& args) {
  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  args.GetReturnValue().Set(WriteFloatGeneric<double, kBigEndian>(args));
}


void ByteLengthUtf8(const FunctionCallbackInfo<Value> &args) {
  CHECK(args[0]->IsString());

  // Fast case: avoid StringBytes on UTF8 string. Jump to v8.
  args.GetReturnValue().Set(args[0].As<String>()->Utf8Length());
}


void Compare(const FunctionCallbackInfo<Value> &args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  THROW_AND_RETURN_UNLESS_BUFFER(env, args[1]);
  SPREAD_ARG(args[0], obj_a);
  SPREAD_ARG(args[1], obj_b);

  size_t cmp_length = MIN(obj_a_length, obj_b_length);

  int val = cmp_length > 0 ? memcmp(obj_a_data, obj_b_data, cmp_length) : 0;

  // Normalize val to be an integer in the range of [1, -1] since
  // implementations of memcmp() can vary by platform.
  if (val == 0) {
    if (obj_a_length > obj_b_length)
      val = 1;
    else if (obj_a_length < obj_b_length)
      val = -1;
  } else {
    if (val > 0)
      val = 1;
    else
      val = -1;
  }

  args.GetReturnValue().Set(val);
}


void IndexOfString(const FunctionCallbackInfo<Value>& args) {
  ASSERT(args[1]->IsString());
  ASSERT(args[2]->IsNumber());

  enum encoding enc = ParseEncoding(args.GetIsolate(),
                                    args[3],
                                    UTF8);

  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  SPREAD_ARG(args[0], ts_obj);

  Local<String> needle = args[1].As<String>();
  const char* haystack = ts_obj_data;
  const size_t haystack_length = ts_obj_length;
  const size_t needle_length = needle->Utf8Length();


  if (needle_length == 0 || haystack_length == 0) {
    return args.GetReturnValue().Set(-1);
  }

  int64_t offset_i64 = args[2]->IntegerValue();
  size_t offset = 0;

  if (offset_i64 < 0) {
    if (offset_i64 + static_cast<int64_t>(haystack_length) < 0) {
      offset = 0;
    } else {
      offset = static_cast<size_t>(haystack_length + offset_i64);
    }
  } else {
    offset = static_cast<size_t>(offset_i64);
  }

  if (haystack_length < offset || needle_length + offset > haystack_length) {
    return args.GetReturnValue().Set(-1);
  }

  size_t result = haystack_length;

  if (enc == UCS2) {
    String::Value needle_value(needle);
    if (*needle_value == nullptr)
      return args.GetReturnValue().Set(-1);

    if (haystack_length < 2 || needle_value.length() < 1) {
      return args.GetReturnValue().Set(-1);
    }

    if (IsBigEndian()) {
      StringBytes::InlineDecoder decoder;
      decoder.Decode(Environment::GetCurrent(args), needle, args[3], UCS2);
      const uint16_t* decoded_string =
          reinterpret_cast<const uint16_t*>(decoder.out());

      if (decoded_string == nullptr)
        return args.GetReturnValue().Set(-1);

      result = SearchString(reinterpret_cast<const uint16_t*>(haystack),
                            haystack_length / 2,
                            decoded_string,
                            decoder.size() / 2,
                            offset / 2);
    } else {
      result = SearchString(reinterpret_cast<const uint16_t*>(haystack),
                            haystack_length / 2,
                            reinterpret_cast<const uint16_t*>(*needle_value),
                            needle_value.length(),
                            offset / 2);
    }
    result *= 2;
  } else if (enc == UTF8) {
    String::Utf8Value needle_value(needle);
    if (*needle_value == nullptr)
      return args.GetReturnValue().Set(-1);

    result = SearchString(reinterpret_cast<const uint8_t*>(haystack),
                          haystack_length,
                          reinterpret_cast<const uint8_t*>(*needle_value),
                          needle_length,
                          offset);
  } else if (enc == BINARY) {
    uint8_t* needle_data = static_cast<uint8_t*>(malloc(needle_length));
    if (needle_data == nullptr) {
      return args.GetReturnValue().Set(-1);
    }
    needle->WriteOneByte(
        needle_data, 0, needle_length, String::NO_NULL_TERMINATION);

    result = SearchString(reinterpret_cast<const uint8_t*>(haystack),
                          haystack_length,
                          needle_data,
                          needle_length,
                          offset);
    free(needle_data);
  }

  args.GetReturnValue().Set(
      result == haystack_length ? -1 : static_cast<int>(result));
}

void IndexOfBuffer(const FunctionCallbackInfo<Value>& args) {
  ASSERT(args[1]->IsObject());
  ASSERT(args[2]->IsNumber());

  enum encoding enc = ParseEncoding(args.GetIsolate(),
                                    args[3],
                                    UTF8);

  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  SPREAD_ARG(args[0], ts_obj);
  SPREAD_ARG(args[1], buf);

  if (buf_length > 0)
    CHECK_NE(buf_data, nullptr);

  const char* haystack = ts_obj_data;
  const size_t haystack_length = ts_obj_length;
  const char* needle = buf_data;
  const size_t needle_length = buf_length;

  if (needle_length == 0 || haystack_length == 0) {
    return args.GetReturnValue().Set(-1);
  }

  int64_t offset_i64 = args[2]->IntegerValue();
  size_t offset = 0;

  if (offset_i64 < 0) {
    if (offset_i64 + static_cast<int64_t>(haystack_length) < 0)
      offset = 0;
    else
      offset = static_cast<size_t>(haystack_length + offset_i64);
  } else {
    offset = static_cast<size_t>(offset_i64);
  }

  if (haystack_length < offset || needle_length + offset > haystack_length) {
    return args.GetReturnValue().Set(-1);
  }

  size_t result = haystack_length;

  if (enc == UCS2) {
    if (haystack_length < 2 || needle_length < 2) {
      return args.GetReturnValue().Set(-1);
    }
    result = SearchString(
        reinterpret_cast<const uint16_t*>(haystack),
        haystack_length / 2,
        reinterpret_cast<const uint16_t*>(needle),
        needle_length / 2,
        offset / 2);
    result *= 2;
  } else {
    result = SearchString(
        reinterpret_cast<const uint8_t*>(haystack),
        haystack_length,
        reinterpret_cast<const uint8_t*>(needle),
        needle_length,
        offset);
  }

  args.GetReturnValue().Set(
      result == haystack_length ? -1 : static_cast<int>(result));
}

void IndexOfNumber(const FunctionCallbackInfo<Value>& args) {
  ASSERT(args[1]->IsNumber());
  ASSERT(args[2]->IsNumber());

  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  SPREAD_ARG(args[0], ts_obj);

  uint32_t needle = args[1]->Uint32Value();
  int64_t offset_i64 = args[2]->IntegerValue();
  size_t offset;

  if (offset_i64 < 0) {
    if (offset_i64 + static_cast<int64_t>(ts_obj_length) < 0)
      offset = 0;
    else
      offset = static_cast<size_t>(ts_obj_length + offset_i64);
  } else {
    offset = static_cast<size_t>(offset_i64);
  }

  if (ts_obj_length == 0 || offset + 1 > ts_obj_length)
    return args.GetReturnValue().Set(-1);

  void* ptr = memchr(ts_obj_data + offset, needle, ts_obj_length - offset);
  char* ptr_char = static_cast<char*>(ptr);
  args.GetReturnValue().Set(ptr ? static_cast<int>(ptr_char - ts_obj_data)
                                : -1);
}


// pass Buffer object to load prototype methods
void SetupBufferJS(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsObject());
  Local<Object> proto = args[0].As<Object>();
  env->set_buffer_prototype_object(proto);

  env->SetMethod(proto, "asciiSlice", AsciiSlice);
  env->SetMethod(proto, "base64Slice", Base64Slice);
  env->SetMethod(proto, "binarySlice", BinarySlice);
  env->SetMethod(proto, "hexSlice", HexSlice);
  env->SetMethod(proto, "ucs2Slice", Ucs2Slice);
  env->SetMethod(proto, "utf8Slice", Utf8Slice);

  env->SetMethod(proto, "asciiWrite", AsciiWrite);
  env->SetMethod(proto, "base64Write", Base64Write);
  env->SetMethod(proto, "binaryWrite", BinaryWrite);
  env->SetMethod(proto, "hexWrite", HexWrite);
  env->SetMethod(proto, "ucs2Write", Ucs2Write);
  env->SetMethod(proto, "utf8Write", Utf8Write);

  env->SetMethod(proto, "copy", Copy);

  CHECK(args[1]->IsObject());
  Local<Object> bObj = args[1].As<Object>();

  uint32_t* const fields = env->array_buffer_allocator_info()->fields();
  uint32_t const fields_count =
      env->array_buffer_allocator_info()->fields_count();

  Local<ArrayBuffer> array_buffer =
      ArrayBuffer::New(env->isolate(), fields, sizeof(*fields) * fields_count);

  bObj->Set(String::NewFromUtf8(env->isolate(), "flags"),
            Uint32Array::New(array_buffer, 0, fields_count));
}


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "setupBufferJS", SetupBufferJS);
  env->SetMethod(target, "createFromString", CreateFromString);
  env->SetMethod(target, "createFromArrayBuffer", CreateFromArrayBuffer);

  env->SetMethod(target, "byteLengthUtf8", ByteLengthUtf8);
  env->SetMethod(target, "compare", Compare);
  env->SetMethod(target, "fill", Fill);
  env->SetMethod(target, "indexOfBuffer", IndexOfBuffer);
  env->SetMethod(target, "indexOfNumber", IndexOfNumber);
  env->SetMethod(target, "indexOfString", IndexOfString);

  env->SetMethod(target, "readDoubleBE", ReadDoubleBE);
  env->SetMethod(target, "readDoubleLE", ReadDoubleLE);
  env->SetMethod(target, "readFloatBE", ReadFloatBE);
  env->SetMethod(target, "readFloatLE", ReadFloatLE);

  env->SetMethod(target, "writeDoubleBE", WriteDoubleBE);
  env->SetMethod(target, "writeDoubleLE", WriteDoubleLE);
  env->SetMethod(target, "writeFloatBE", WriteFloatBE);
  env->SetMethod(target, "writeFloatLE", WriteFloatLE);

  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "kMaxLength"),
              Integer::NewFromUnsigned(env->isolate(), kMaxLength)).FromJust();

  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "kStringMaxLength"),
              Integer::New(env->isolate(), String::kMaxLength)).FromJust();
}


}  // namespace Buffer
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(buffer, node::Buffer::Initialize)
