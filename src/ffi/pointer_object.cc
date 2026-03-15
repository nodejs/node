#if HAVE_FFI

#include "../base_object-inl.h"
#include "../env-inl.h"
#include "../node_errors.h"
#include "node_ffi.h"

#include <cstdio>
#include <cstring>
#include <iostream>

namespace node {

using v8::BigInt;
using v8::IntegrityLevel;
using v8::Isolate;
using v8::Local;
using v8::Null;
using v8::Object;
using v8::Value;

namespace ffi {

Local<Object> CreatePointerObject(Environment* env, void* address) {
  Isolate* isolate = env->isolate();
  Local<Object> obj = Object::New(isolate, Null(isolate), nullptr, nullptr, 0);

  obj->SetPrivate(
         env->context(),
         env->ffi_pointer_address_private_symbol(),
         BigInt::NewFromUnsigned(isolate, reinterpret_cast<uint64_t>(address)))
      .Check();
  obj->SetIntegrityLevel(env->context(), IntegrityLevel::kFrozen).Check();

  return obj;
}

bool IsPointerObject(Environment* env, Local<Value> value) {
  if (!value->IsObject()) {
    return false;
  }

  bool has;
  if (!value.As<Object>()->HasPrivate(env->context(), env->ffi_pointer_address_private_symbol()).To(&has)) {
    return false;
  }
  return has;
}

bool GetPointerAddress(Environment* env,
                       Local<Value> value,
                       uint64_t* address) {
  if (!value->IsObject()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"value\" argument must be a pointer object.");
    return false;
  }

  Local<Value> addr_val;
  if (!value.As<Object>()->GetPrivate(env->context(),
                                      env->ffi_pointer_address_private_symbol())
           .ToLocal(&addr_val)) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"value\" argument does not contain an address.");
    return false;
  }

  if (!addr_val->IsBigInt()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"value\" argument does not contain a BigInt.");
    return false;
  }

  auto big = addr_val.As<BigInt>();
  *address = big->Uint64Value();
  return true;
}

}  // namespace ffi
}  // namespace node

#endif  // HAVE_FFI
