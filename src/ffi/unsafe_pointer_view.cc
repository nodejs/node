#if HAVE_FFI

#include "../base_object-inl.h"
#include "../env-inl.h"
#include "../node_errors.h"
#include "../permission/permission.h"
#include "node_ffi.h"

#include <cstring>

namespace node {

using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::BigInt;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Value;

namespace ffi {

// UnsafePointerView implementation
Local<FunctionTemplate> UnsafePointerView::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl =
      env->ffi_unsafe_pointer_view_constructor_template();

  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, UnsafePointerView::New);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        UnsafePointerView::kInternalFieldCount);

    // DataView methods
    SetProtoMethod(isolate, tmpl, "getInt8", UnsafePointerView::GetInt8);
    SetProtoMethod(isolate, tmpl, "getUint8", UnsafePointerView::GetUint8);
    SetProtoMethod(isolate, tmpl, "getInt16", UnsafePointerView::GetInt16);
    SetProtoMethod(isolate, tmpl, "getUint16", UnsafePointerView::GetUint16);
    SetProtoMethod(isolate, tmpl, "getInt32", UnsafePointerView::GetInt32);
    SetProtoMethod(isolate, tmpl, "getUint32", UnsafePointerView::GetUint32);
    SetProtoMethod(
        isolate, tmpl, "getBigInt64", UnsafePointerView::GetBigInt64);
    SetProtoMethod(
        isolate, tmpl, "getBigUint64", UnsafePointerView::GetBigUint64);
    SetProtoMethod(isolate, tmpl, "getFloat32", UnsafePointerView::GetFloat32);
    SetProtoMethod(isolate, tmpl, "getFloat64", UnsafePointerView::GetFloat64);

    SetProtoMethod(isolate, tmpl, "setInt8", UnsafePointerView::SetInt8);
    SetProtoMethod(isolate, tmpl, "setUint8", UnsafePointerView::SetUint8);
    SetProtoMethod(isolate, tmpl, "setInt16", UnsafePointerView::SetInt16);
    SetProtoMethod(isolate, tmpl, "setUint16", UnsafePointerView::SetUint16);
    SetProtoMethod(isolate, tmpl, "setInt32", UnsafePointerView::SetInt32);
    SetProtoMethod(isolate, tmpl, "setUint32", UnsafePointerView::SetUint32);
    SetProtoMethod(
        isolate, tmpl, "setBigInt64", UnsafePointerView::SetBigInt64);
    SetProtoMethod(
        isolate, tmpl, "setBigUint64", UnsafePointerView::SetBigUint64);
    SetProtoMethod(isolate, tmpl, "setFloat32", UnsafePointerView::SetFloat32);
    SetProtoMethod(isolate, tmpl, "setFloat64", UnsafePointerView::SetFloat64);

    SetProtoMethod(isolate, tmpl, "getBool", UnsafePointerView::GetBool);
    SetProtoMethod(isolate, tmpl, "getCString", UnsafePointerView::GetCString);
    SetProtoMethod(isolate, tmpl, "getPointer", UnsafePointerView::GetPointer);
    SetProtoMethod(isolate, tmpl, "copyInto", UnsafePointerView::CopyInto);
    SetProtoMethod(
        isolate, tmpl, "getArrayBuffer", UnsafePointerView::GetArrayBuffer);

    env->set_ffi_unsafe_pointer_view_constructor_template(tmpl);
  }
  return tmpl;
}

UnsafePointerView::UnsafePointerView(Environment* env,
                                     Local<Object> object,
                                     void* pointer)
    : BaseObject(env, object), pointer_(pointer) {
  MakeWeak();
}

UnsafePointerView::~UnsafePointerView() {}

void UnsafePointerView::MemoryInfo(MemoryTracker* tracker) const {
  // Pointer itself is tracked as part of the object
}

void UnsafePointerView::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

  if (!IsPointerObject(env, args[0])) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"pointer\" argument must be a pointer object.");
    return;
  }

  uint64_t address = 0;
  if (!GetPointerAddress(env, args[0], &address)) {
    return;
  }

  void* ptr = reinterpret_cast<void*>(address);
  new UnsafePointerView(env, args.This(), ptr);
}

void UnsafePointerView::GetInt8(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  int8_t value =
      *reinterpret_cast<int8_t*>(static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(value);
}

void UnsafePointerView::GetUint8(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  uint8_t value =
      *reinterpret_cast<uint8_t*>(static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(value);
}

void UnsafePointerView::GetInt16(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  int16_t value =
      *reinterpret_cast<int16_t*>(static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(value);
}

void UnsafePointerView::GetUint16(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  uint16_t value =
      *reinterpret_cast<uint16_t*>(static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(value);
}

void UnsafePointerView::GetInt32(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  int32_t value =
      *reinterpret_cast<int32_t*>(static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(value);
}

void UnsafePointerView::GetUint32(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  uint32_t value =
      *reinterpret_cast<uint32_t*>(static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(value);
}

void UnsafePointerView::GetBigInt64(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  int64_t value =
      *reinterpret_cast<int64_t*>(static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(BigInt::New(env->isolate(), value));
}

void UnsafePointerView::GetBigUint64(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  uint64_t value =
      *reinterpret_cast<uint64_t*>(static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(BigInt::NewFromUnsigned(env->isolate(), value));
}

void UnsafePointerView::GetFloat32(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  float value =
      *reinterpret_cast<float*>(static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(static_cast<double>(value));
}

void UnsafePointerView::GetFloat64(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  double value =
      *reinterpret_cast<double*>(static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(value);
}

void UnsafePointerView::SetInt8(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1) {
    env->ThrowTypeError("Expected a value argument");
    return;
  }
  auto offset = 0;
  if (args.Length() < 2) {
    env->ThrowTypeError("Expected value and optional offset arguments");
    return;
  }
  if (args[1]->IsNumber()) {
    offset = static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
  } else if (args[1]->IsBigInt()) {
    auto big = args[1].As<BigInt>();
    offset = static_cast<int>(big->Int64Value());
  } else {
    env->ThrowTypeError("Offset must be a number or BigInt");
    return;
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  int8_t value =
      static_cast<int8_t>(args[0]->Int32Value(env->context()).FromJust());
  *reinterpret_cast<int8_t*>(static_cast<char*>(view->pointer_) + offset) =
      value;
}

void UnsafePointerView::SetUint8(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1) {
    env->ThrowTypeError("Expected a value argument");
    return;
  }
  auto offset = 0;
  if (args.Length() < 2) {
    env->ThrowTypeError("Expected value and optional offset arguments");
    return;
  }
  if (args[1]->IsNumber()) {
    offset = static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
  } else if (args[1]->IsBigInt()) {
    auto big = args[1].As<BigInt>();
    offset = static_cast<int>(big->Int64Value());
  } else {
    env->ThrowTypeError("Offset must be a number or BigInt");
    return;
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  uint8_t value =
      static_cast<uint8_t>(args[0]->Uint32Value(env->context()).FromJust());
  *reinterpret_cast<uint8_t*>(static_cast<char*>(view->pointer_) + offset) =
      value;
}

void UnsafePointerView::SetInt16(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1) {
    env->ThrowTypeError("Expected a value argument");
    return;
  }
  auto offset = 0;
  if (args.Length() < 2) {
    env->ThrowTypeError("Expected value and optional offset arguments");
    return;
  }
  if (args[1]->IsNumber()) {
    offset = static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
  } else if (args[1]->IsBigInt()) {
    auto big = args[1].As<BigInt>();
    offset = static_cast<int>(big->Int64Value());
  } else {
    env->ThrowTypeError("Offset must be a number or BigInt");
    return;
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  int16_t value =
      static_cast<int16_t>(args[0]->Int32Value(env->context()).FromJust());
  *reinterpret_cast<int16_t*>(static_cast<char*>(view->pointer_) + offset) =
      value;
}

void UnsafePointerView::SetUint16(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1) {
    env->ThrowTypeError("Expected a value argument");
    return;
  }
  auto offset = 0;
  if (args.Length() < 2) {
    env->ThrowTypeError("Expected value and optional offset arguments");
    return;
  }
  if (args[1]->IsNumber()) {
    offset = static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
  } else if (args[1]->IsBigInt()) {
    auto big = args[1].As<BigInt>();
    offset = static_cast<int>(big->Int64Value());
  } else {
    env->ThrowTypeError("Offset must be a number or BigInt");
    return;
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  uint16_t value =
      static_cast<uint16_t>(args[0]->Uint32Value(env->context()).FromJust());
  *reinterpret_cast<uint16_t*>(static_cast<char*>(view->pointer_) + offset) =
      value;
}

void UnsafePointerView::SetInt32(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1) {
    env->ThrowTypeError("Expected a value argument");
    return;
  }
  auto offset = 0;
  if (args.Length() < 2) {
    env->ThrowTypeError("Expected value and optional offset arguments");
    return;
  }
  if (args[1]->IsNumber()) {
    offset = static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
  } else if (args[1]->IsBigInt()) {
    auto big = args[1].As<BigInt>();
    offset = static_cast<int>(big->Int64Value());
  } else {
    env->ThrowTypeError("Offset must be a number or BigInt");
    return;
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  int32_t value = args[0]->Int32Value(env->context()).FromJust();
  *reinterpret_cast<int32_t*>(static_cast<char*>(view->pointer_) + offset) =
      value;
}

void UnsafePointerView::SetUint32(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1) {
    env->ThrowTypeError("Expected a value argument");
    return;
  }
  auto offset = 0;
  if (args.Length() < 2) {
    env->ThrowTypeError("Expected value and optional offset arguments");
    return;
  }
  if (args[1]->IsNumber()) {
    offset = static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
  } else if (args[1]->IsBigInt()) {
    auto big = args[1].As<BigInt>();
    offset = static_cast<int>(big->Int64Value());
  } else {
    env->ThrowTypeError("Offset must be a number or BigInt");
    return;
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  uint32_t value = args[0]->Uint32Value(env->context()).FromJust();
  *reinterpret_cast<uint32_t*>(static_cast<char*>(view->pointer_) + offset) =
      value;
}

void UnsafePointerView::SetBigInt64(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1) {
    env->ThrowTypeError("Expected a value argument");
    return;
  }
  auto offset = 0;
  if (args.Length() < 2) {
    env->ThrowTypeError("Expected value and optional offset arguments");
    return;
  }
  if (args[1]->IsNumber()) {
    offset = static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
  } else if (args[1]->IsBigInt()) {
    auto big = args[1].As<BigInt>();
    offset = static_cast<int>(big->Int64Value());
  } else {
    env->ThrowTypeError("Offset must be a number or BigInt");
    return;
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  auto big = args[0]->ToBigInt(env->context());
  if (big.IsEmpty()) {
    env->ThrowTypeError("Invalid BigInt value");
    return;
  }
  int64_t value = big.ToLocalChecked()->Int64Value();
  *reinterpret_cast<int64_t*>(static_cast<char*>(view->pointer_) + offset) =
      value;
}

void UnsafePointerView::SetBigUint64(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1) {
    env->ThrowTypeError("Expected a value argument");
    return;
  }
  auto offset = 0;
  if (args.Length() < 2) {
    env->ThrowTypeError("Expected value and optional offset arguments");
    return;
  }
  if (args[1]->IsNumber()) {
    offset = static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
  } else if (args[1]->IsBigInt()) {
    auto big = args[1].As<BigInt>();
    offset = static_cast<int>(big->Int64Value());
  } else {
    env->ThrowTypeError("Offset must be a number or BigInt");
    return;
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  auto big = args[0]->ToBigInt(env->context());
  if (big.IsEmpty()) {
    env->ThrowTypeError("Invalid BigInt value");
    return;
  }
  uint64_t value = big.ToLocalChecked()->Uint64Value();
  *reinterpret_cast<uint64_t*>(static_cast<char*>(view->pointer_) + offset) =
      value;
}

void UnsafePointerView::SetFloat32(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1) {
    env->ThrowTypeError("Expected a value argument");
    return;
  }
  auto offset = 0;
  if (args.Length() < 2) {
    env->ThrowTypeError("Expected value and optional offset arguments");
    return;
  }
  if (args[1]->IsNumber()) {
    offset = static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
  } else if (args[1]->IsBigInt()) {
    auto big = args[1].As<BigInt>();
    offset = static_cast<int>(big->Int64Value());
  } else {
    env->ThrowTypeError("Offset must be a number or BigInt");
    return;
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  float value =
      static_cast<float>(args[0]->NumberValue(env->context()).FromJust());
  *reinterpret_cast<float*>(static_cast<char*>(view->pointer_) + offset) =
      value;
}

void UnsafePointerView::SetFloat64(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1) {
    env->ThrowTypeError("Expected a value argument");
    return;
  }
  auto offset = 0;
  if (args.Length() < 2) {
    env->ThrowTypeError("Expected value and optional offset arguments");
    return;
  }
  if (args[1]->IsNumber()) {
    offset = static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
  } else if (args[1]->IsBigInt()) {
    auto big = args[1].As<BigInt>();
    offset = static_cast<int>(big->Int64Value());
  } else {
    env->ThrowTypeError("Offset must be a number or BigInt");
    return;
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  double value = args[0]->NumberValue(env->context()).FromJust();
  *reinterpret_cast<double*>(static_cast<char*>(view->pointer_) + offset) =
      value;
}

void UnsafePointerView::GetBool(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  bool value =
      *reinterpret_cast<bool*>(static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(value);
}

void UnsafePointerView::GetCString(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  const char* str = reinterpret_cast<const char*>(
      static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(), str, NewStringType::kNormal)
          .ToLocalChecked());
}

void UnsafePointerView::GetPointer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto offset = 0;
  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      offset =
          static_cast<int>(args[0]->NumberValue(env->context()).FromJust());
    } else if (args[0]->IsBigInt()) {
      auto big = args[0].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }
  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  void* ptr =
      *reinterpret_cast<void**>(static_cast<char*>(view->pointer_) + offset);
  args.GetReturnValue().Set(CreatePointerObject(env, ptr));
}

void UnsafePointerView::CopyInto(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 1 || !args[0]->IsArrayBufferView()) {
    env->ThrowTypeError("First argument must be an ArrayBufferView");
    return;
  }

  auto offset = 0;
  if (args.Length() > 1) {
    if (args[1]->IsNumber()) {
      offset =
          static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
    } else if (args[1]->IsBigInt()) {
      auto big = args[1].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }

  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  Local<ArrayBufferView> dest = args[0].As<ArrayBufferView>();
  size_t byte_length = dest->ByteLength();
  std::shared_ptr<BackingStore> store = dest->Buffer()->GetBackingStore();
  if (!store) {
    env->ThrowTypeError("Invalid ArrayBufferView backing store");
    return;
  }
  void* dest_ptr = store->Data();
  size_t dest_offset = dest->ByteOffset();
  void* src = static_cast<char*>(view->pointer_) + offset;

  std::memcpy(static_cast<char*>(dest_ptr) + dest_offset, src, byte_length);
}

void UnsafePointerView::GetArrayBuffer(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 1 || !args[0]->IsNumber()) {
    env->ThrowTypeError("byteLength argument must be a number");
    return;
  }

  size_t byte_length =
      static_cast<size_t>(args[0]->NumberValue(env->context()).FromJust());
  auto offset = 0;
  if (args.Length() > 1) {
    if (args[1]->IsNumber()) {
      offset =
          static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
    } else if (args[1]->IsBigInt()) {
      auto big = args[1].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }

  UnsafePointerView* view = Unwrap<UnsafePointerView>(args.This());
  void* ptr = static_cast<char*>(view->pointer_) + offset;

  auto backing_store = ArrayBuffer::NewBackingStore(
      ptr, byte_length, [](void*, size_t, void*) {}, nullptr);

  Local<ArrayBuffer> array_buffer =
      ArrayBuffer::New(env->isolate(), std::move(backing_store));
  args.GetReturnValue().Set(array_buffer);
}

void UnsafePointerView::StaticGetCString(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

  if (!IsPointerObject(env, args[0])) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"pointer\" argument must be a pointer object.");
    return;
  }

  uint64_t address = 0;
  if (!GetPointerAddress(env, args[0], &address)) {
    return;
  }

  auto offset = 0;
  if (args.Length() > 1) {
    if (args[1]->IsNumber()) {
      offset =
          static_cast<int>(args[1]->NumberValue(env->context()).FromJust());
    } else if (args[1]->IsBigInt()) {
      auto big = args[1].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }

  void* ptr = reinterpret_cast<void*>(address);
  const char* str =
      reinterpret_cast<const char*>(static_cast<char*>(ptr) + offset);
  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(), str, NewStringType::kNormal)
          .ToLocalChecked());
}

void UnsafePointerView::StaticCopyInto(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

  if (!IsPointerObject(env, args[0])) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"pointer\" argument must be a pointer object.");
    return;
  }

  if (!args[1]->IsArrayBufferView()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"destination\" argument must be an ArrayBufferView.");
    return;
  }

  uint64_t address = 0;
  if (!GetPointerAddress(env, args[0], &address)) {
    return;
  }

  auto offset = 0;
  if (args.Length() > 2) {
    if (args[2]->IsNumber()) {
      offset =
          static_cast<int>(args[2]->NumberValue(env->context()).FromJust());
    } else if (args[2]->IsBigInt()) {
      auto big = args[2].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }

  void* ptr = reinterpret_cast<void*>(address);
  Local<ArrayBufferView> dest = args[1].As<ArrayBufferView>();
  size_t byte_length = dest->ByteLength();
  std::shared_ptr<BackingStore> store = dest->Buffer()->GetBackingStore();
  if (!store) {
    env->ThrowTypeError("Invalid ArrayBufferView backing store");
    return;
  }
  void* dest_ptr = store->Data();
  size_t dest_offset = dest->ByteOffset();
  void* src = static_cast<char*>(ptr) + offset;

  std::memcpy(static_cast<char*>(dest_ptr) + dest_offset, src, byte_length);
}

void UnsafePointerView::StaticGetArrayBuffer(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

  if (!IsPointerObject(env, args[0])) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"pointer\" argument must be a pointer object.");
    return;
  }

  if (!args[1]->IsNumber()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"byteLength\" argument must be a number.");
    return;
  }

  uint64_t address = 0;
  if (!GetPointerAddress(env, args[0], &address)) {
    return;
  }

  size_t byte_length =
      static_cast<size_t>(args[1]->NumberValue(env->context()).FromJust());
  auto offset = 0;
  if (args.Length() > 2) {
    if (args[2]->IsNumber()) {
      offset =
          static_cast<int>(args[2]->NumberValue(env->context()).FromJust());
    } else if (args[2]->IsBigInt()) {
      auto big = args[2].As<BigInt>();
      offset = static_cast<int>(big->Int64Value());
    } else {
      env->ThrowTypeError("Offset must be a number or BigInt");
      return;
    }
  }

  void* ptr = reinterpret_cast<void*>(address);
  void* final_ptr = static_cast<char*>(ptr) + offset;

  auto backing_store = ArrayBuffer::NewBackingStore(
      final_ptr, byte_length, [](void*, size_t, void*) {}, nullptr);

  Local<ArrayBuffer> array_buffer =
      ArrayBuffer::New(env->isolate(), std::move(backing_store));
  args.GetReturnValue().Set(array_buffer);
}

}  // namespace ffi
}  // namespace node

#endif  // HAVE_FFI
