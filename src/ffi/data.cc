#if HAVE_FFI

#include "data.h"
#include "base_object-inl.h"
#include "node_errors.h"
#include "util.h"
#include "v8.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::BigInt;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

namespace node {

namespace ffi {

bool GetValidatedSize(Environment* env,
                      Local<Value> value,
                      const char* label,
                      size_t* out) {
  if (!value->IsNumber()) {
    THROW_ERR_INVALID_ARG_VALUE(env, "The %s must be a number", label);
    return false;
  }

  double length = value.As<Number>()->Value();
  if (!std::isfinite(length) || length < 0 || std::floor(length) != length) {
    THROW_ERR_INVALID_ARG_VALUE(
        env, "The %s must be a non-negative integer", label);
    return false;
  }

  if (length > static_cast<double>(std::numeric_limits<size_t>::max())) {
    THROW_ERR_OUT_OF_RANGE(env, "The %s is too large", label);
    return false;
  }

  *out = static_cast<size_t>(length);
  return true;
}

bool GetValidatedPointerAddress(Environment* env,
                                Local<Value> value,
                                const char* label,
                                uintptr_t* out) {
  if (!value->IsBigInt()) {
    THROW_ERR_INVALID_ARG_VALUE(env, "The %s must be a bigint", label);
    return false;
  }

  bool lossless;
  uint64_t address = value.As<BigInt>()->Uint64Value(&lossless);
  if (!lossless) {
    THROW_ERR_INVALID_ARG_VALUE(
        env, "The %s must be a non-negative bigint", label);
    return false;
  }

  if (address > static_cast<uint64_t>(std::numeric_limits<uintptr_t>::max())) {
    THROW_ERR_INVALID_ARG_VALUE(
        env, "The %s exceeds the platform pointer range", label);
    return false;
  }

  *out = static_cast<uintptr_t>(address);

  return true;
}

bool GetValidatedSignedInt(Environment* env,
                           Local<Value> value,
                           int64_t min,
                           int64_t max,
                           const char* type_name,
                           int64_t* out) {
  if (!value->IsNumber()) {
    THROW_ERR_INVALID_ARG_VALUE(
        env, (std::string("Value must be an ") + type_name).c_str());
    return false;
  }

  double number = value.As<Number>()->Value();
  if (!std::isfinite(number) || std::floor(number) != number || number < min ||
      number > max) {
    THROW_ERR_INVALID_ARG_VALUE(
        env, (std::string("Value must be an ") + type_name).c_str());
    return false;
  }

  *out = static_cast<int64_t>(number);
  return true;
}

bool GetValidatedUnsignedInt(Environment* env,
                             Local<Value> value,
                             uint64_t max,
                             const char* type_name,
                             uint64_t* out) {
  if (!value->IsNumber()) {
    THROW_ERR_INVALID_ARG_VALUE(
        env, (std::string("Value must be a ") + type_name).c_str());
    return false;
  }

  double number = value.As<Number>()->Value();
  if (!std::isfinite(number) || std::floor(number) != number || number < 0 ||
      number > static_cast<double>(max)) {
    THROW_ERR_INVALID_ARG_VALUE(
        env, (std::string("Value must be a ") + type_name).c_str());
    return false;
  }

  *out = static_cast<uint64_t>(number);
  return true;
}

bool ValidatePointerSpan(Environment* env,
                         uintptr_t raw_ptr,
                         size_t offset,
                         size_t length,
                         const char* error_message) {
  if (offset > std::numeric_limits<uintptr_t>::max() - raw_ptr) {
    THROW_ERR_INVALID_ARG_VALUE(env, error_message);
    return false;
  }

  uintptr_t start = raw_ptr + offset;
  if (length > 0 &&
      length - 1 > std::numeric_limits<uintptr_t>::max() - start) {
    THROW_ERR_INVALID_ARG_VALUE(env, error_message);
    return false;
  }

  return true;
}

bool ValidateBufferLength(Environment* env, size_t len) {
  if (len > Buffer::kMaxLength) {
    THROW_ERR_BUFFER_TOO_LARGE(env, "Buffer is too large");
    return false;
  }

  return true;
}

bool ValidateStringLength(Environment* env, size_t len) {
  if (len > static_cast<size_t>(String::kMaxLength)) {
    THROW_ERR_STRING_TOO_LONG(env, "String is too long");
    return false;
  }

  return true;
}

bool GetValidatedPointerAndOffset(Environment* env,
                                  const FunctionCallbackInfo<Value>& args,
                                  uint8_t** ptr,
                                  size_t* offset) {
  uintptr_t raw_ptr;
  if (args.Length() < 1 ||
      !GetValidatedPointerAddress(env, args[0], "pointer", &raw_ptr)) {
    return false;
  }

  if (raw_ptr == 0) {
    THROW_ERR_FFI_INVALID_POINTER(env, "Cannot dereference a null pointer");
    return false;
  }

  *offset = 0;
  if (args.Length() > 1 && !args[1]->IsUndefined()) {
    if (!GetValidatedSize(env, args[1], "offset", offset)) {
      return false;
    }
  }

  if (!ValidatePointerSpan(
          env,
          raw_ptr,
          *offset,
          1,
          "The pointer and offset exceed the platform address range")) {
    return false;
  }

  *ptr = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(raw_ptr));
  return true;
}

bool GetValidatedPointerValueAndOffset(Environment* env,
                                       const FunctionCallbackInfo<Value>& args,
                                       uint8_t** ptr,
                                       Local<Value>* value,
                                       size_t* offset) {
  uintptr_t raw_ptr;
  if (args.Length() < 1 ||
      !GetValidatedPointerAddress(env, args[0], "pointer", &raw_ptr)) {
    return false;
  }

  if (raw_ptr == 0) {
    THROW_ERR_FFI_INVALID_POINTER(env, "Cannot dereference a null pointer");
    return false;
  }

  if (args.Length() < 2 || args[1]->IsUndefined()) {
    THROW_ERR_INVALID_ARG_VALUE(env, "Expected an offset argument");
    return false;
  }

  if (!GetValidatedSize(env, args[1], "offset", offset)) {
    return false;
  }

  if (!ValidatePointerSpan(
          env,
          raw_ptr,
          *offset,
          1,
          "The pointer and offset exceed the platform address range")) {
    return false;
  }

  if (args.Length() < 3 || args[2]->IsUndefined()) {
    THROW_ERR_INVALID_ARG_VALUE(env, "Expected a value argument");
    return false;
  }

  *value = args[2];

  *ptr = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(raw_ptr));
  return true;
}

template <typename T>
void GetValue(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");
  Isolate* isolate = env->isolate();
  uint8_t* ptr;
  size_t offset;

  if (!GetValidatedPointerAndOffset(env, args, &ptr, &offset)) {
    return;
  }

  uintptr_t raw_ptr = reinterpret_cast<uintptr_t>(ptr);
  if (!ValidatePointerSpan(
          env,
          raw_ptr,
          offset,
          sizeof(T),
          "The accessed range exceeds the platform address range")) {
    return;
  }

  T value;
  std::memcpy(&value, ptr + offset, sizeof(value));
  if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> ||
                std::is_same_v<T, int32_t>) {
    args.GetReturnValue().Set(Integer::New(isolate, value));
  } else if constexpr (std::is_same_v<T, uint8_t> ||
                       std::is_same_v<T, uint16_t> ||
                       std::is_same_v<T, uint32_t>) {
    args.GetReturnValue().Set(Integer::NewFromUnsigned(isolate, value));
  } else if constexpr (std::is_same_v<T, int64_t>) {
    args.GetReturnValue().Set(BigInt::New(isolate, value));
  } else if constexpr (std::is_same_v<T, uint64_t>) {
    args.GetReturnValue().Set(BigInt::NewFromUnsigned(isolate, value));
  } else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
    args.GetReturnValue().Set(Number::New(isolate, value));
  }
}

template <typename T>
void SetValue(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");
  uint8_t* ptr;
  Local<Value> value;
  size_t offset;

  if (!GetValidatedPointerValueAndOffset(env, args, &ptr, &value, &offset)) {
    return;
  }

  uintptr_t raw_ptr = reinterpret_cast<uintptr_t>(ptr);
  if (!ValidatePointerSpan(
          env,
          raw_ptr,
          offset,
          sizeof(T),
          "The accessed range exceeds the platform address range")) {
    return;
  }

  T converted;
  Local<Context> context = env->context();

  if constexpr (std::is_same_v<T, int8_t>) {
    int64_t validated;
    if (!GetValidatedSignedInt(
            env, value, INT8_MIN, INT8_MAX, "int8", &validated)) {
      return;
    }
    converted = static_cast<T>(validated);
  } else if constexpr (std::is_same_v<T, uint8_t>) {
    uint64_t validated;
    if (!GetValidatedUnsignedInt(env, value, UINT8_MAX, "uint8", &validated)) {
      return;
    }
    converted = static_cast<T>(validated);
  } else if constexpr (std::is_same_v<T, int16_t>) {
    int64_t validated;
    if (!GetValidatedSignedInt(
            env, value, INT16_MIN, INT16_MAX, "int16", &validated)) {
      return;
    }
    converted = static_cast<T>(validated);
  } else if constexpr (std::is_same_v<T, uint16_t>) {
    uint64_t validated;
    if (!GetValidatedUnsignedInt(
            env, value, UINT16_MAX, "uint16", &validated)) {
      return;
    }
    converted = static_cast<T>(validated);
  } else if constexpr (std::is_same_v<T, int32_t>) {
    int64_t validated;
    if (!GetValidatedSignedInt(
            env, value, INT32_MIN, INT32_MAX, "int32", &validated)) {
      return;
    }
    converted = static_cast<T>(validated);
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    uint64_t validated;
    if (!GetValidatedUnsignedInt(
            env, value, UINT32_MAX, "uint32", &validated)) {
      return;
    }
    converted = static_cast<T>(validated);
  } else if constexpr (std::is_same_v<T, int64_t>) {
    if (value->IsBigInt()) {
      bool lossless;
      converted = static_cast<T>(value.As<BigInt>()->Int64Value(&lossless));
      if (!lossless) {
        THROW_ERR_INVALID_ARG_VALUE(env, "Value must be an int64");
        return;
      }
    } else if (value->IsNumber()) {
      int64_t validated;
      if (!GetValidatedSignedInt(env,
                                 value,
                                 -static_cast<int64_t>(kMaxSafeJsInteger),
                                 static_cast<int64_t>(kMaxSafeJsInteger),
                                 "int64",
                                 &validated)) {
        return;
      }
      converted = static_cast<T>(validated);
    } else {
      THROW_ERR_INVALID_ARG_VALUE(env, "Value must be a bigint or a number");
      return;
    }
  } else if constexpr (std::is_same_v<T, uint64_t>) {
    if (value->IsBigInt()) {
      bool lossless;
      converted = static_cast<T>(value.As<BigInt>()->Uint64Value(&lossless));
      if (!lossless) {
        THROW_ERR_INVALID_ARG_VALUE(env, "Value must be a uint64");
        return;
      }
    } else if (value->IsNumber()) {
      uint64_t validated;
      if (!GetValidatedUnsignedInt(env,
                                   value,
                                   static_cast<uint64_t>(kMaxSafeJsInteger),
                                   "uint64",
                                   &validated)) {
        return;
      }
      converted = static_cast<T>(validated);
    } else {
      THROW_ERR_INVALID_ARG_VALUE(env, "Value must be a bigint or a number");
      return;
    }
  } else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
    MaybeLocal<Number> number = value->ToNumber(context);
    Local<Number> number_local;

    if (!number.ToLocal(&number_local)) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Value must be a number");
      return;
    }

    converted = static_cast<T>(number_local->Value());
  }

  std::memcpy(ptr + offset, &converted, sizeof(converted));
}

// Raw FFI memory helpers are low-level and unsafe. Pointer validity,
// lifetime, and zero-copy writable views are the caller's responsibility.
void GetInt8(const FunctionCallbackInfo<Value>& args) {
  GetValue<int8_t>(args);
}

void GetUint8(const FunctionCallbackInfo<Value>& args) {
  GetValue<uint8_t>(args);
}

void GetInt16(const FunctionCallbackInfo<Value>& args) {
  GetValue<int16_t>(args);
}

void GetUint16(const FunctionCallbackInfo<Value>& args) {
  GetValue<uint16_t>(args);
}

void GetInt32(const FunctionCallbackInfo<Value>& args) {
  GetValue<int32_t>(args);
}

void GetUint32(const FunctionCallbackInfo<Value>& args) {
  GetValue<uint32_t>(args);
}

void GetInt64(const FunctionCallbackInfo<Value>& args) {
  GetValue<int64_t>(args);
}

void GetUint64(const FunctionCallbackInfo<Value>& args) {
  GetValue<uint64_t>(args);
}

void GetFloat32(const FunctionCallbackInfo<Value>& args) {
  GetValue<float>(args);
}

void GetFloat64(const FunctionCallbackInfo<Value>& args) {
  GetValue<double>(args);
}

void SetInt8(const FunctionCallbackInfo<Value>& args) {
  SetValue<int8_t>(args);
}

void SetUint8(const FunctionCallbackInfo<Value>& args) {
  SetValue<uint8_t>(args);
}

void SetInt16(const FunctionCallbackInfo<Value>& args) {
  SetValue<int16_t>(args);
}

void SetUint16(const FunctionCallbackInfo<Value>& args) {
  SetValue<uint16_t>(args);
}

void SetInt32(const FunctionCallbackInfo<Value>& args) {
  SetValue<int32_t>(args);
}

void SetUint32(const FunctionCallbackInfo<Value>& args) {
  SetValue<uint32_t>(args);
}

void SetInt64(const FunctionCallbackInfo<Value>& args) {
  SetValue<int64_t>(args);
}

void SetUint64(const FunctionCallbackInfo<Value>& args) {
  SetValue<uint64_t>(args);
}

void SetFloat32(const FunctionCallbackInfo<Value>& args) {
  SetValue<float>(args);
}

void SetFloat64(const FunctionCallbackInfo<Value>& args) {
  SetValue<double>(args);
}

void ToString(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  // Raw FFI memory helpers are low-level and unsafe. Pointer validity,
  // lifetime, and zero-copy writable views are the caller's responsibility.
  // `toString()` requires a valid NUL-terminated C string pointer.
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

  if (args.Length() < 1 || !args[0]->IsBigInt()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "The first argument must be a bigint");
    return;
  }

  uintptr_t ptr;
  if (!GetValidatedPointerAddress(env, args[0], "first argument", &ptr)) {
    return;
  }

  if (ptr == 0) {
    args.GetReturnValue().SetNull();
    return;
  }

  const char* str = reinterpret_cast<const char*>(ptr);
  size_t len = std::strlen(str);
  if (!ValidateStringLength(env, len)) {
    return;
  }

  Local<String> out;
  if (!String::NewFromUtf8(isolate, str, NewStringType::kNormal)
           .ToLocal(&out)) {
    return;
  }

  args.GetReturnValue().Set(out);
}

void ToBuffer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

  // `copy === false` exposes a zero-copy writable view over foreign memory.
  // This is intentionally unsafe and the caller must guarantee pointer
  // validity, lifetime, and bounds.

  if (args.Length() < 1 || !args[0]->IsBigInt()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "The first argument must be a bigint");
    return;
  }

  uintptr_t ptr;
  if (!GetValidatedPointerAddress(env, args[0], "first argument", &ptr)) {
    return;
  }

  size_t len;
  if (args.Length() < 2 || !GetValidatedSize(env, args[1], "length", &len)) {
    return;
  }

  if (ptr == 0 && len > 0) {
    THROW_ERR_FFI_INVALID_POINTER(env,
                                  "Cannot create a buffer from a null pointer");
    return;
  }

  if (!ValidatePointerSpan(
          env,
          ptr,
          0,
          len,
          "The pointer and length exceed the platform address range")) {
    return;
  }

  if (!ValidateBufferLength(env, len)) {
    return;
  }

  Local<Object> buf;
  if (args.Length() < 3 || args[2]->IsUndefined() ||
      args[2]->BooleanValue(isolate)) {
    if (!Buffer::Copy(env, reinterpret_cast<char*>(ptr), len).ToLocal(&buf)) {
      return;
    }
  } else {
    if (!Buffer::New(
             env,
             reinterpret_cast<char*>(ptr),
             len,
             [](char* data, void* hint) {},
             nullptr)
             .ToLocal(&buf)) {
      return;
    }
  }

  args.GetReturnValue().Set(buf);
}

void ToArrayBuffer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

  if (args.Length() < 1 || !args[0]->IsBigInt()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "The first argument must be a bigint");
    return;
  }

  uintptr_t ptr;
  if (!GetValidatedPointerAddress(env, args[0], "first argument", &ptr)) {
    return;
  }

  size_t len;
  if (args.Length() < 2 || !GetValidatedSize(env, args[1], "length", &len)) {
    return;
  }

  if (ptr == 0 && len > 0) {
    THROW_ERR_FFI_INVALID_POINTER(
        env, "Cannot create an ArrayBuffer from a null pointer");
    return;
  }

  if (!ValidatePointerSpan(
          env,
          ptr,
          0,
          len,
          "The pointer and length exceed the platform address range")) {
    return;
  }

  if (!ValidateBufferLength(env, len)) {
    return;
  }

  Local<ArrayBuffer> ab;

  if (args.Length() < 3 || args[2]->IsUndefined() ||
      args[2]->BooleanValue(isolate)) {
    std::unique_ptr<BackingStore> store =
        ArrayBuffer::NewBackingStore(isolate, len);
    memcpy(store->Data(), reinterpret_cast<void*>(ptr), len);
    ab = ArrayBuffer::New(isolate, std::move(store));
  } else {
    std::unique_ptr<BackingStore> store = ArrayBuffer::NewBackingStore(
        reinterpret_cast<void*>(ptr),
        len,
        [](void* data, size_t length, void* deleter_data) {},
        nullptr);

    ab = ArrayBuffer::New(isolate, std::move(store));
  }

  args.GetReturnValue().Set(ab);
}

void ExportBytes(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

  if (args.Length() < 1) {
    THROW_ERR_INVALID_ARG_TYPE(
        env,
        "The first argument must be a Buffer, ArrayBuffer, or ArrayBufferView");
    return;
  }

  const uint8_t* source_data = nullptr;
  size_t source_len = 0;

  if (args[0]->IsArrayBuffer()) {
    Local<ArrayBuffer> array_buffer = args[0].As<ArrayBuffer>();
    std::shared_ptr<BackingStore> store = array_buffer->GetBackingStore();
    if (!store) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Invalid ArrayBuffer backing store");
      return;
    }
    source_data = static_cast<const uint8_t*>(store->Data());
    source_len = array_buffer->ByteLength();
  } else if (args[0]->IsArrayBufferView()) {
    ArrayBufferViewContents<uint8_t> view(args[0]);
    if (view.WasDetached()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Invalid ArrayBufferView backing store");
      return;
    }
    source_data = view.data();
    source_len = view.length();
  } else {
    THROW_ERR_INVALID_ARG_TYPE(
        env,
        "The first argument must be a Buffer, ArrayBuffer, or ArrayBufferView");
    return;
  }

  uintptr_t ptr;
  if (args.Length() < 2 ||
      !GetValidatedPointerAddress(env, args[1], "pointer", &ptr)) {
    return;
  }

  size_t len;
  if (args.Length() < 3 || !GetValidatedSize(env, args[2], "length", &len)) {
    return;
  }

  if (len < source_len) {
    THROW_ERR_OUT_OF_RANGE(env, "The length must be >= source byte length");
    return;
  }

  if (ptr == 0 && source_len > 0) {
    THROW_ERR_FFI_INVALID_POINTER(env,
                                  "Cannot create a buffer from a null pointer");
    return;
  }

  if (!ValidatePointerSpan(
          env,
          ptr,
          0,
          len,
          "The pointer and length exceed the platform address range")) {
    return;
  }

  if (source_len > 0) {
    std::memcpy(reinterpret_cast<void*>(ptr), source_data, source_len);
  }
}

void GetRawPointer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

  if (args.Length() < 1) {
    THROW_ERR_INVALID_ARG_TYPE(
        env,
        "The first argument must be a Buffer, ArrayBuffer, or ArrayBufferView");
    return;
  }

  uintptr_t ptr = 0;

  if (args[0]->IsArrayBuffer()) {
    Local<ArrayBuffer> array_buffer = args[0].As<ArrayBuffer>();
    std::shared_ptr<BackingStore> store = array_buffer->GetBackingStore();
    if (!store) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Invalid ArrayBuffer backing store");
      return;
    }
    ptr = reinterpret_cast<uintptr_t>(store->Data());
  } else if (args[0]->IsArrayBufferView()) {
    ArrayBufferViewContents<uint8_t> view(args[0]);
    if (view.WasDetached()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Invalid ArrayBufferView backing store");
      return;
    }
    ptr = reinterpret_cast<uintptr_t>(view.data());
  } else {
    THROW_ERR_INVALID_ARG_TYPE(
        env,
        "The first argument must be a Buffer, ArrayBuffer, or ArrayBufferView");
    return;
  }

  args.GetReturnValue().Set(
      BigInt::NewFromUnsigned(isolate, static_cast<uint64_t>(ptr)));
}

}  // namespace ffi

}  // namespace node

#endif  // HAVE_FFI
