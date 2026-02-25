#if HAVE_FFI

#include "../base_object-inl.h"
#include "../env-inl.h"
#include "../node_errors.h"
#include "../permission/permission.h"
#include "node_ffi.h"

namespace node {

using v8::BigInt;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

namespace ffi {

Local<FunctionTemplate> UnsafePointer::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl = env->ffi_unsafe_pointer_constructor_template();

  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, UnsafePointer::New);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        UnsafePointer::kInternalFieldCount);

    // Static methods.
    SetMethod(isolate, tmpl, "create", UnsafePointer::Create);
    SetMethodNoSideEffect(isolate, tmpl, "equals", UnsafePointer::Equals);
    SetMethod(isolate, tmpl, "offset", UnsafePointer::Offset);
    SetMethodNoSideEffect(isolate, tmpl, "value", UnsafePointer::GetValue);

    env->set_ffi_unsafe_pointer_constructor_template(tmpl);
  }
  return tmpl;
}

UnsafePointer::UnsafePointer(Environment* env, Local<Object> object, void* ptr)
    : BaseObject(env, object), pointer_(ptr) {
  MakeWeak();
}

UnsafePointer::~UnsafePointer() {}

void UnsafePointer::MemoryInfo(MemoryTracker* tracker) const {}

void UnsafePointer::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

  if (!args[0]->IsBigInt()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"value\" argument must be a BigInt.");
    return;
  }

  auto value = args[0].As<BigInt>();
  void* ptr = reinterpret_cast<void*>(value->Uint64Value());
  new UnsafePointer(env, args.This(), ptr);
}

void UnsafePointer::Create(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

  if (!args[0]->IsBigInt()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"value\" argument must be a BigInt.");
    return;
  }

  auto value = args[0].As<BigInt>();
  void* ptr = reinterpret_cast<void*>(value->Uint64Value());
  args.GetReturnValue().Set(CreatePointerObject(env, ptr));
}

void UnsafePointer::GetValue(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!IsPointerObject(env, args[0])) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"pointer\" argument must be a pointer object.");
    return;
  }

  uint64_t address = 0;
  if (!GetPointerAddress(env, args[0], &address)) {
    return;
  }

  args.GetReturnValue().Set(BigInt::NewFromUnsigned(env->isolate(), address));
}

void UnsafePointer::Offset(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!IsPointerObject(env, args[0])) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"pointer\" argument must be a pointer object.");
    return;
  }

  if (!args[1]->IsBigInt() && !args[1]->IsNumber()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"offset\" argument must be a number or BigInt.");
    return;
  }

  uint64_t address = 0;
  if (!GetPointerAddress(env, args[0], &address)) {
    return;
  }

  int64_t offset = 0;
  if (args[1]->IsBigInt()) {
    offset = args[1].As<BigInt>()->Int64Value();
  } else {
    offset =
        static_cast<int64_t>(args[1]->NumberValue(env->context()).FromJust());
  }

  void* ptr = reinterpret_cast<void*>(address);
  void* new_ptr = static_cast<char*>(ptr) + offset;
  args.GetReturnValue().Set(CreatePointerObject(env, new_ptr));
}

void UnsafePointer::Equals(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!IsPointerObject(env, args[0])) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"first\" argument must be a pointer object.");
    return;
  }

  if (!IsPointerObject(env, args[1])) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"second\" argument must be a pointer object.");
    return;
  }

  uint64_t address_a = 0;
  if (!GetPointerAddress(env, args[0], &address_a)) {
    return;
  }

  uint64_t address_b = 0;
  if (!GetPointerAddress(env, args[1], &address_b)) {
    return;
  }

  args.GetReturnValue().Set(address_a == address_b);
}

}  // namespace ffi
}  // namespace node

#endif  // HAVE_FFI
