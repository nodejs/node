#include "node.h"
#include "node_buffer.h"

#include "env.h"
#include "env-inl.h"
#include "smalloc.h"
#include "string_bytes.h"
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

#define ARGS_THIS_DEC(name)                                                 \
  size_t name##_length;                                                     \
  char* name##_data;

#define SPREAD_ARG(val, name)                                                 \
  CHECK((val)->IsUint8Array());                                               \
  Local<Uint8Array> name = (val).As<Uint8Array>();                            \
  ArrayBuffer::Contents name##_c = name->Buffer()->GetContents();             \
  size_t name##_offset = name->ByteOffset();                                  \
  name##_length = name->ByteLength();                                         \
  name##_data = static_cast<char*>(name##_c.Data()) + name##_offset;          \
  if (name##_length > 0)                                                      \
    CHECK_NE(name##_data, nullptr);

#define ARGS_THIS(argT, name)                                                 \
  Local<Object> name = argT;                                                  \
  name##_length = name->GetIndexedPropertiesExternalArrayDataLength();        \
  name##_data = static_cast<char*>(                                           \
    name->GetIndexedPropertiesExternalArrayData());                           \
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
using v8::Handle;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::Number;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::Uint32;
using v8::Uint8Array;
using v8::Value;
using v8::WeakCallbackData;


class CallbackInfo {
 public:
  static inline void Free(char* data, void* hint);
  static inline CallbackInfo* New(Isolate* isolate,
                                  Handle<Object> object,
                                  FreeCallback callback,
                                  void* hint = 0);
  inline void Dispose(Isolate* isolate);
  inline Persistent<Object>* persistent();
 private:
  static void WeakCallback(const WeakCallbackData<Object, CallbackInfo>&);
  inline void WeakCallback(Isolate* isolate, Local<Object> object);
  inline CallbackInfo(Isolate* isolate,
                      Handle<Object> object,
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
                                Handle<Object> object,
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
                           Handle<Object> object,
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
  ARGS_THIS_DEC(obj);
  SPREAD_ARG(object, obj);
  CHECK_EQ(obj_offset, 0);
  CHECK_EQ(obj_c.ByteLength(), obj_length);

  obj->Buffer()->Neuter();
  callback_(obj_data, hint_);
  int64_t change_in_bytes = -static_cast<int64_t>(sizeof(*this));
  isolate->AdjustAmountOfExternalAllocatedMemory(change_in_bytes);

  delete this;
}


// Buffer methods

bool HasInstance(Handle<Value> val) {
  return val->IsObject() && HasInstance(val.As<Object>());
}


bool HasInstance(Handle<Object> obj) {
  if (using_old_buffer) {
    if (!obj->HasIndexedPropertiesInExternalArrayData())
      return false;
    v8::ExternalArrayType type =
        obj->GetIndexedPropertiesExternalArrayDataType();
    return type == v8::kExternalUint8Array;
  }

  if (!obj->IsUint8Array())
    return false;
  Local<Uint8Array> array = obj.As<Uint8Array>();
  Environment* env = Environment::GetCurrent(array->GetIsolate());
  return array->GetPrototype()->StrictEquals(env->buffer_prototype_object());
}


char* Data(Handle<Value> val) {
  CHECK(val->IsObject());
  // Use a fully qualified name here to work around a bug in gcc 4.2.
  // It mistakes an unadorned call to Data() for the v8::String::Data type.
  return node::Buffer::Data(val.As<Object>());
}


char* Data(Handle<Object> obj) {
  if (using_old_buffer) {
    CHECK(obj->HasIndexedPropertiesInExternalArrayData());
    return static_cast<char*>(obj->GetIndexedPropertiesExternalArrayData());
  }

  CHECK(obj->IsUint8Array());
  Local<Uint8Array> ui = obj.As<Uint8Array>();
  ArrayBuffer::Contents ab_c = ui->Buffer()->GetContents();
  return static_cast<char*>(ab_c.Data()) + ui->ByteOffset();
}


size_t Length(Handle<Value> val) {
  CHECK(val->IsObject());
  return Length(val.As<Object>());
}


size_t Length(Handle<Object> obj) {
  if (using_old_buffer) {
    CHECK(obj->HasIndexedPropertiesInExternalArrayData());
    return obj->GetIndexedPropertiesExternalArrayDataLength();
  }

  CHECK(obj->IsUint8Array());
  Local<Uint8Array> ui = obj.As<Uint8Array>();
  return ui->ByteLength();
}


Local<Object> New(Isolate* isolate, Handle<String> string, enum encoding enc) {
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

  Local<Object> buf = Use(isolate, data, actual);
  return scope.Escape(buf);
}


Local<Object> New(Isolate* isolate, size_t length) {
  EscapableHandleScope handle_scope(isolate);
  Local<Object> obj = Buffer::New(Environment::GetCurrent(isolate), length);
  return handle_scope.Escape(obj);
}


Local<Object> New(Environment* env, size_t length) {
  EscapableHandleScope scope(env->isolate());

  if (using_old_buffer) {
    CHECK_LE(length, kMaxLength);

    Local<Value> arg = Uint32::NewFromUnsigned(env->isolate(), length);
    Local<Object> obj =
        env->buffer_constructor_function()->NewInstance(1, &arg);

    smalloc::Alloc(env, obj, length);

    return scope.Escape(obj);
  }

  // V8 currently only allows a maximum Typed Array index of max Smi.
  if (!IsValidSmi(length)) {
    return Local<Object>();
  }

  void* data;
  if (length > 0) {
    data = malloc(length);
    // NOTE: API change. Must check .IsEmpty() on the return object to see if
    // the data was able to be allocated.
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
  if (!mb.FromMaybe(false)) {
    FatalError("node::Buffer::New(Environment*, size_t)",
               "Could not set Object prototype");
    UNREACHABLE();
  }
  return scope.Escape(ui);
}


Local<Object> New(Isolate* isolate, const char* data, size_t length) {
  Environment* env = Environment::GetCurrent(isolate);
  EscapableHandleScope handle_scope(env->isolate());
  Local<Object> obj = Buffer::New(env, data, length);
  return handle_scope.Escape(obj);
}


// Make a copy of "data". Why this isn't called "Copy", we'll never know.
Local<Object> New(Environment* env, const char* data, size_t length) {
  EscapableHandleScope scope(env->isolate());

  if (using_old_buffer) {
    CHECK_LE(length, kMaxLength);

    Local<Value> arg = Uint32::NewFromUnsigned(env->isolate(), length);
    Local<Object> obj =
        env->buffer_constructor_function()->NewInstance(1, &arg);

    char* new_data;
    if (length > 0) {
      new_data = static_cast<char*>(malloc(length));
      if (new_data == nullptr)
        FatalError("node::Buffer::New(const char*, size_t)", "Out Of Memory");
      memcpy(new_data, data, length);
    } else {
      new_data = nullptr;
    }

    smalloc::Alloc(env, obj, new_data, length);

    return scope.Escape(obj);
  }

  // V8 currently only allows a maximum Typed Array index of max Smi.
  if (!IsValidSmi(length)) {
    return Local<Object>();
  }

  void* new_data;
  if (length > 0) {
    CHECK_NE(data, nullptr);
    new_data = malloc(length);
    // NOTE: API change. Must check .IsEmpty() on the return object to see if
    // the data was able to be allocated.
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
  if (!mb.FromMaybe(false)) {
    FatalError("node::Buffer::New(Environment*, char*, size_t)",
               "Could not set Object prototype");
    UNREACHABLE();
  }

  return scope.Escape(ui);
}


Local<Object> New(Isolate* isolate,
                  char* data,
                  size_t length,
                  FreeCallback callback,
                  void* hint) {
  Environment* env = Environment::GetCurrent(isolate);
  EscapableHandleScope handle_scope(env->isolate());
  Local<Object> obj = Buffer::New(env, data, length, callback, hint);
  return handle_scope.Escape(obj);
}


Local<Object> New(Environment* env,
                  char* data,
                  size_t length,
                  FreeCallback callback,
                  void* hint) {
  EscapableHandleScope scope(env->isolate());

  if (using_old_buffer) {
    CHECK_LE(length, kMaxLength);
    Local<Value> arg = Uint32::NewFromUnsigned(env->isolate(), length);
    Local<Object> obj =
        env->buffer_constructor_function()->NewInstance(1, &arg);
    smalloc::Alloc(env, obj, data, length, callback, hint);
    return scope.Escape(obj);
  }

  if (!IsValidSmi(length)) {
    return Local<Object>();
  }

  Local<ArrayBuffer> ab = ArrayBuffer::New(env->isolate(), data, length);
  Local<Uint8Array> ui = Uint8Array::New(ab, 0, length);
  Maybe<bool> mb =
      ui->SetPrototype(env->context(), env->buffer_prototype_object());
  if (!mb.FromMaybe(false)) {
    FatalError("node::Buffer::New(Environment*, char*, size_t,"
               " FreeCallback, void*)",
               "Could not set Object prototype");
    UNREACHABLE();
  }
  CallbackInfo::New(env->isolate(), ui, callback, hint);
  return scope.Escape(ui);
}


Local<Object> Use(Isolate* isolate, char* data, size_t length) {
  Environment* env = Environment::GetCurrent(isolate);
  EscapableHandleScope handle_scope(env->isolate());
  Local<Object> obj = Buffer::Use(env, data, length);
  return handle_scope.Escape(obj);
}


Local<Object> Use(Environment* env, char* data, size_t length) {
  EscapableHandleScope scope(env->isolate());

  if (using_old_buffer) {
    CHECK_LE(length, kMaxLength);

    Local<Value> arg = Uint32::NewFromUnsigned(env->isolate(), length);
    Local<Object> obj =
        env->buffer_constructor_function()->NewInstance(1, &arg);

    smalloc::Alloc(env, obj, data, length);

    return scope.Escape(obj);
  }

  if (length > 0) {
    CHECK_NE(data, nullptr);
    CHECK(IsValidSmi(length));
  }

  Local<ArrayBuffer> ab =
      ArrayBuffer::New(env->isolate(),
                       data,
                       length,
                       ArrayBufferCreationMode::kInternalized);
  Local<Uint8Array> ui = Uint8Array::New(ab, 0, length);
  Maybe<bool> mb =
      ui->SetPrototype(env->context(), env->buffer_prototype_object());
  if (!mb.FromMaybe(false)) {
    FatalError("node::Buffer::Use(Environment*, char*, size_t)",
               "Could not set Object prototype");
    UNREACHABLE();
  }
  return scope.Escape(ui);
}


void Create(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsNumber());

  int64_t length = args[0]->IntegerValue();

  if (!IsValidSmi(length)) {
    return env->ThrowRangeError("invalid Buffer length");
  }

  void* data;
  if (length > 0) {
    data = malloc(length);
    if (data == nullptr)
      return env->ThrowRangeError("invalid Buffer length");
  } else {
    data = nullptr;
  }

  Local<ArrayBuffer> ab =
      ArrayBuffer::New(isolate,
                       data,
                       length,
                       ArrayBufferCreationMode::kInternalized);
  Local<Uint8Array> ui = Uint8Array::New(ab, 0, length);
  Maybe<bool> mb =
      ui->SetPrototype(env->context(), env->buffer_prototype_object());
  if (mb.FromMaybe(false))
    args.GetReturnValue().Set(ui);
}


void CreateFromString(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());

  enum encoding enc = ParseEncoding(args.GetIsolate(),
                                    args[1].As<String>(),
                                    UTF8);
  Local<Object> buf = New(args.GetIsolate(), args[0].As<String>(), enc);
  args.GetReturnValue().Set(buf);
}


void Slice(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsUint8Array());
  CHECK(args[1]->IsNumber());
  CHECK(args[2]->IsNumber());
  Environment* env = Environment::GetCurrent(args);
  Local<Uint8Array> ab_ui = args[0].As<Uint8Array>();
  Local<ArrayBuffer> ab = ab_ui->Buffer();
  ArrayBuffer::Contents ab_c = ab->GetContents();
  size_t offset = ab_ui->ByteOffset();
  size_t start = args[1]->NumberValue() + offset;
  size_t end = args[2]->NumberValue() + offset;
  CHECK_GE(end, start);
  size_t size = end - start;
  CHECK_GE(ab_c.ByteLength(), start + size);
  Local<Uint8Array> ui = Uint8Array::New(ab, start, size);
  Maybe<bool> mb =
      ui->SetPrototype(env->context(), env->buffer_prototype_object());
  if (mb.FromMaybe(false))
    args.GetReturnValue().Set(ui);
}


template <encoding encoding>
void StringSlice(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  THROW_AND_RETURN_UNLESS_BUFFER(env, args.This());

  ARGS_THIS_DEC(ts_obj);

  if (using_old_buffer) {
    ARGS_THIS(args.This(), ts_obj);
  } else {
    SPREAD_ARG(args.This(), ts_obj);
  }

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

  ARGS_THIS_DEC(ts_obj);

  if (using_old_buffer) {
    ARGS_THIS(args.This(), ts_obj);
  } else {
    SPREAD_ARG(args.This(), ts_obj);
  }

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

  Local<Object> target_obj = args[0]->ToObject(env->isolate());

  ARGS_THIS_DEC(ts_obj);
  ARGS_THIS_DEC(target);

  if (using_old_buffer) {
    ARGS_THIS(args.This(), ts_obj);
    target_length = target_obj->GetIndexedPropertiesExternalArrayDataLength();
    target_data = static_cast<char*>(
        target_obj->GetIndexedPropertiesExternalArrayData());
  } else {
    SPREAD_ARG(args.This(), ts_obj);
    SPREAD_ARG(target_obj, target);
  }

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
  ARGS_THIS_DEC(ts_obj);

  if (using_old_buffer) {
    ARGS_THIS(args[0].As<Object>(), ts_obj);
  } else {
    SPREAD_ARG(args[0], ts_obj);
  }

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
  ARGS_THIS_DEC(ts_obj);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args.This());

  if (using_old_buffer) {
    ARGS_THIS(args.This(), ts_obj);
  } else {
    SPREAD_ARG(args.This(), ts_obj);
  }

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
  ARGS_THIS_DEC(ts_obj);

  if (using_old_buffer) {
    ARGS_THIS(args[0].As<Object>(), ts_obj);
  } else {
    SPREAD_ARG(args[0], ts_obj);
  }

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
  ARGS_THIS_DEC(ts_obj);

  if (using_old_buffer) {
    ARGS_THIS(args[0].As<Object>(), ts_obj);
  } else {
    SPREAD_ARG(args[0], ts_obj);
  }

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

  ARGS_THIS_DEC(obj_a);
  ARGS_THIS_DEC(obj_b);

  if (using_old_buffer) {
    ARGS_THIS(args[0].As<Object>(), obj_a);
    ARGS_THIS(args[1].As<Object>(), obj_b);
  } else {
    SPREAD_ARG(args[0], obj_a);
    SPREAD_ARG(args[1], obj_b);
  }

  size_t cmp_length = MIN(obj_a_length, obj_b_length);

  int32_t val = memcmp(obj_a_data, obj_b_data, cmp_length);

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


int32_t IndexOf(const char* haystack,
                size_t h_length,
                const char* needle,
                size_t n_length) {
  CHECK_GE(h_length, n_length);
  // TODO(trevnorris): Implement Boyer-Moore string search algorithm.
  for (size_t i = 0; i < h_length - n_length + 1; i++) {
    if (haystack[i] == needle[0]) {
      if (memcmp(haystack + i, needle, n_length) == 0)
        return i;
    }
  }
  return -1;
}


void IndexOfString(const FunctionCallbackInfo<Value>& args) {
  ASSERT(args[1]->IsString());
  ASSERT(args[2]->IsNumber());

  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  ARGS_THIS_DEC(ts_obj);

  if (using_old_buffer) {
    ARGS_THIS(args[0].As<Object>(), ts_obj);
  } else {
    SPREAD_ARG(args[0], ts_obj);
  }

  node::Utf8Value str(args.GetIsolate(), args[1]);
  int32_t offset_i32 = args[2]->Int32Value();
  uint32_t offset;

  if (offset_i32 < 0) {
    if (offset_i32 + static_cast<int32_t>(ts_obj_length) < 0)
      offset = 0;
    else
      offset = static_cast<uint32_t>(ts_obj_length + offset_i32);
  } else {
    offset = static_cast<uint32_t>(offset_i32);
  }

  if (str.length() == 0 ||
      ts_obj_length == 0 ||
      (offset != 0 && str.length() + offset <= str.length()) ||
      str.length() + offset > ts_obj_length)
    return args.GetReturnValue().Set(-1);

  int32_t r =
      IndexOf(ts_obj_data + offset, ts_obj_length - offset, *str, str.length());
  args.GetReturnValue().Set(r == -1 ? -1 : static_cast<int32_t>(r + offset));
}


void IndexOfBuffer(const FunctionCallbackInfo<Value>& args) {
  ASSERT(args[1]->IsObject());
  ASSERT(args[2]->IsNumber());

  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  ARGS_THIS_DEC(ts_obj);

  if (using_old_buffer) {
    ARGS_THIS(args[0].As<Object>(), ts_obj);
  } else {
    SPREAD_ARG(args[0], ts_obj);
  }

  Local<Object> buf = args[1].As<Object>();
  int32_t offset_i32 = args[2]->Int32Value();
  size_t buf_length = buf->GetIndexedPropertiesExternalArrayDataLength();
  char* buf_data =
      static_cast<char*>(buf->GetIndexedPropertiesExternalArrayData());
  uint32_t offset;

  if (buf_length > 0)
    CHECK_NE(buf_data, nullptr);

  if (offset_i32 < 0) {
    if (offset_i32 + static_cast<int32_t>(ts_obj_length) < 0)
      offset = 0;
    else
      offset = static_cast<uint32_t>(ts_obj_length + offset_i32);
  } else {
    offset = static_cast<uint32_t>(offset_i32);
  }

  if (buf_length == 0 ||
      ts_obj_length == 0 ||
      (offset != 0 && buf_length + offset <= buf_length) ||
      buf_length + offset > ts_obj_length)
    return args.GetReturnValue().Set(-1);

  int32_t r =
    IndexOf(ts_obj_data + offset, ts_obj_length - offset, buf_data, buf_length);
  args.GetReturnValue().Set(r == -1 ? -1 : static_cast<int32_t>(r + offset));
}


void IndexOfNumber(const FunctionCallbackInfo<Value>& args) {
  ASSERT(args[1]->IsNumber());
  ASSERT(args[2]->IsNumber());

  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  ARGS_THIS_DEC(ts_obj);

  if (using_old_buffer) {
    ARGS_THIS(args[0].As<Object>(), ts_obj);
  } else {
    SPREAD_ARG(args[0], ts_obj);
  }

  uint32_t needle = args[1]->Uint32Value();
  int32_t offset_i32 = args[2]->Int32Value();
  uint32_t offset;

  if (offset_i32 < 0) {
    if (offset_i32 + static_cast<int32_t>(ts_obj_length) < 0)
      offset = 0;
    else
      offset = static_cast<uint32_t>(ts_obj_length + offset_i32);
  } else {
    offset = static_cast<uint32_t>(offset_i32);
  }

  if (ts_obj_length == 0 || offset + 1 > ts_obj_length)
    return args.GetReturnValue().Set(-1);

  void* ptr = memchr(ts_obj_data + offset, needle, ts_obj_length - offset);
  char* ptr_char = static_cast<char*>(ptr);
  args.GetReturnValue().Set(
      ptr ? static_cast<int32_t>(ptr_char - ts_obj_data) : -1);
}


// pass Buffer object to load prototype methods
void SetupBufferJS(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Local<Object> proto;

  if (using_old_buffer) {
    CHECK(args[0]->IsFunction());
    Local<Function> bv = args[0].As<Function>();
    env->set_buffer_constructor_function(bv);
    Local<Value> proto_v = bv->Get(env->prototype_string());
    CHECK(proto_v->IsObject());
    proto = proto_v.As<Object>();
  } else {
    CHECK(args[0]->IsObject());
    proto = args[0].As<Object>();
    env->set_buffer_prototype_object(proto);
  }

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

  // for backwards compatibility
  if (using_old_buffer) {
    proto->ForceSet(env->offset_string(),
                    Uint32::New(env->isolate(), 0),
                    v8::ReadOnly);
  }
}


void Initialize(Handle<Object> target,
                Handle<Value> unused,
                Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "setupBufferJS", SetupBufferJS);
  env->SetMethod(target, "create", Create);
  env->SetMethod(target, "createFromString", CreateFromString);

  env->SetMethod(target, "slice", Slice);
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
}


}  // namespace Buffer
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(buffer, node::Buffer::Initialize)
