// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_API_ARGUMENTS_INL_H_
#define V8_API_API_ARGUMENTS_INL_H_

#include "src/api/api-arguments.h"
#include "src/api/api-inl.h"
#include "src/debug/debug.h"
#include "src/execution/vm-state-inl.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/instance-type.h"
#include "src/objects/slots-inl.h"

namespace v8 {
namespace internal {

#if DEBUG
bool IsApiCallResultType(Tagged<Object> obj) {
  if (IsSmi(obj)) return true;
  DCHECK(IsHeapObject(obj));
  return (IsString(obj) || IsSymbol(obj) || IsJSReceiver(obj) ||
          IsHeapNumber(obj) || IsBigInt(obj) || IsUndefined(obj) ||
          IsTrue(obj) || IsFalse(obj) || IsNull(obj));
}
#endif  // DEBUG

CustomArgumentsBase::CustomArgumentsBase(Isolate* isolate)
    : Relocatable(isolate) {}

template <typename T>
CustomArguments<T>::~CustomArguments() {
  slot_at(kReturnValueIndex).store(Object(kHandleZapValue));
}

template <typename T>
template <typename V>
Handle<V> CustomArguments<T>::GetReturnValue(Isolate* isolate) const {
  // Check the ReturnValue.
  FullObjectSlot slot = slot_at(kReturnValueIndex);
  // Nothing was set, return empty handle as per previous behaviour.
  Tagged<Object> raw_object = *slot;
  if (IsTheHole(raw_object, isolate)) return Handle<V>();
  DCHECK(IsApiCallResultType(raw_object));
  return Handle<V>::cast(Handle<Object>(slot.location()));
}

inline Tagged<JSObject> PropertyCallbackArguments::holder() const {
  return JSObject::cast(*slot_at(T::kHolderIndex));
}

inline Tagged<Object> PropertyCallbackArguments::receiver() const {
  return *slot_at(T::kThisIndex);
}

inline Tagged<JSReceiver> FunctionCallbackArguments::holder() const {
  return JSReceiver::cast(*slot_at(T::kHolderIndex));
}

#define DCHECK_NAME_COMPATIBLE(interceptor, name) \
  DCHECK(interceptor->is_named());                \
  DCHECK(!name->IsPrivate());                     \
  DCHECK_IMPLIES(IsSymbol(*name), interceptor->can_intercept_symbols());

#define PREPARE_CALLBACK_INFO_ACCESSOR(ISOLATE, F, API_RETURN_TYPE,            \
                                       ACCESSOR_INFO, RECEIVER, ACCESSOR_KIND) \
  if (ISOLATE->should_check_side_effects() &&                                  \
      !ISOLATE->debug()->PerformSideEffectCheckForAccessor(                    \
          ACCESSOR_INFO, RECEIVER, ACCESSOR_KIND)) {                           \
    return {};                                                                 \
  }                                                                            \
  ExternalCallbackScope call_scope(ISOLATE, FUNCTION_ADDR(F));                 \
  PropertyCallbackInfo<API_RETURN_TYPE> callback_info(values_);

#define PREPARE_CALLBACK_INFO_INTERCEPTOR(ISOLATE, F, API_RETURN_TYPE, \
                                          INTERCEPTOR_INFO)            \
  if (ISOLATE->should_check_side_effects() &&                          \
      !ISOLATE->debug()->PerformSideEffectCheckForInterceptor(         \
          INTERCEPTOR_INFO)) {                                         \
    return {};                                                         \
  }                                                                    \
  ExternalCallbackScope call_scope(ISOLATE, FUNCTION_ADDR(F));         \
  PropertyCallbackInfo<API_RETURN_TYPE> callback_info(values_);

Handle<Object> FunctionCallbackArguments::Call(
    Tagged<CallHandlerInfo> handler) {
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kFunctionCallback);
  v8::FunctionCallback f =
      reinterpret_cast<v8::FunctionCallback>(handler->callback(isolate));
  Handle<Object> receiver_check_unsupported;
  if (isolate->should_check_side_effects() &&
      !isolate->debug()->PerformSideEffectCheckForCallback(
          handle(handler, isolate))) {
    return {};
  }
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));
  FunctionCallbackInfo<v8::Value> info(values_, argv_, argc_);
  f(info);
  return GetReturnValue<Object>(isolate);
}

PropertyCallbackArguments::~PropertyCallbackArguments(){
#ifdef DEBUG
// TODO(chromium:1310062): enable this check.
// if (javascript_execution_counter_) {
//   CHECK_WITH_MSG(javascript_execution_counter_ ==
//                      isolate()->javascript_execution_counter(),
//                  "Unexpected side effect detected");
// }
#endif  // DEBUG
}

// -------------------------------------------------------------------------
// Named Interceptor callbacks.

Handle<JSObject> PropertyCallbackArguments::CallNamedEnumerator(
    Handle<InterceptorInfo> interceptor) {
  DCHECK(interceptor->is_named());
  RCS_SCOPE(isolate(), RuntimeCallCounterId::kNamedEnumeratorCallback);
  return CallPropertyEnumerator(interceptor);
}

Handle<Object> PropertyCallbackArguments::CallNamedQuery(
    Handle<InterceptorInfo> interceptor, Handle<Name> name) {
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedQueryCallback);
  Handle<Object> receiver_check_unsupported;
  GenericNamedPropertyQueryCallback f =
      ToCData<GenericNamedPropertyQueryCallback>(interceptor->query());
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Integer, interceptor);
  f(v8::Utils::ToLocal(name), callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallNamedGetter(
    Handle<InterceptorInfo> interceptor, Handle<Name> name) {
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedGetterCallback);
  GenericNamedPropertyGetterCallback f =
      ToCData<GenericNamedPropertyGetterCallback>(interceptor->getter());
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Value, interceptor);
  f(v8::Utils::ToLocal(name), callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallNamedDescriptor(
    Handle<InterceptorInfo> interceptor, Handle<Name> name) {
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedDescriptorCallback);
  GenericNamedPropertyDescriptorCallback f =
      ToCData<GenericNamedPropertyDescriptorCallback>(
          interceptor->descriptor());
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Value, interceptor);
  f(v8::Utils::ToLocal(name), callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallNamedSetter(
    Handle<InterceptorInfo> interceptor, Handle<Name> name,
    Handle<Object> value) {
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedSetterCallback);
  GenericNamedPropertySetterCallback f =
      ToCData<GenericNamedPropertySetterCallback>(interceptor->setter());
  Handle<InterceptorInfo> has_side_effects;
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Value, has_side_effects);
  f(v8::Utils::ToLocal(name), v8::Utils::ToLocal(value), callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallNamedDefiner(
    Handle<InterceptorInfo> interceptor, Handle<Name> name,
    const v8::PropertyDescriptor& desc) {
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedDefinerCallback);
  GenericNamedPropertyDefinerCallback f =
      ToCData<GenericNamedPropertyDefinerCallback>(interceptor->definer());
  Handle<InterceptorInfo> has_side_effects;
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Value, has_side_effects);
  f(v8::Utils::ToLocal(name), desc, callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallNamedDeleter(
    Handle<InterceptorInfo> interceptor, Handle<Name> name) {
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedDeleterCallback);
  GenericNamedPropertyDeleterCallback f =
      ToCData<GenericNamedPropertyDeleterCallback>(interceptor->deleter());
  Handle<InterceptorInfo> has_side_effects;
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Boolean, has_side_effects);
  f(v8::Utils::ToLocal(name), callback_info);
  return GetReturnValue<Object>(isolate);
}

// -------------------------------------------------------------------------
// Indexed Interceptor callbacks.

Handle<JSObject> PropertyCallbackArguments::CallIndexedEnumerator(
    Handle<InterceptorInfo> interceptor) {
  DCHECK(!interceptor->is_named());
  RCS_SCOPE(isolate(), RuntimeCallCounterId::kIndexedEnumeratorCallback);
  return CallPropertyEnumerator(interceptor);
}

Handle<Object> PropertyCallbackArguments::CallIndexedQuery(
    Handle<InterceptorInfo> interceptor, uint32_t index) {
  DCHECK(!interceptor->is_named());
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexedQueryCallback);
  IndexedPropertyQueryCallback f =
      ToCData<IndexedPropertyQueryCallback>(interceptor->query());
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Integer, interceptor);
  f(index, callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallIndexedGetter(
    Handle<InterceptorInfo> interceptor, uint32_t index) {
  DCHECK(!interceptor->is_named());
  RCS_SCOPE(isolate(), RuntimeCallCounterId::kNamedGetterCallback);
  IndexedPropertyGetterCallback f =
      ToCData<IndexedPropertyGetterCallback>(interceptor->getter());
  Isolate* isolate = this->isolate();
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Value, interceptor);
  f(index, callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallIndexedDescriptor(
    Handle<InterceptorInfo> interceptor, uint32_t index) {
  DCHECK(!interceptor->is_named());
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexedDescriptorCallback);
  IndexedPropertyDescriptorCallback f =
      ToCData<IndexedPropertyDescriptorCallback>(interceptor->descriptor());
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Value, interceptor);
  f(index, callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallIndexedSetter(
    Handle<InterceptorInfo> interceptor, uint32_t index, Handle<Object> value) {
  DCHECK(!interceptor->is_named());
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexedSetterCallback);
  IndexedPropertySetterCallback f =
      ToCData<IndexedPropertySetterCallback>(interceptor->setter());
  Handle<InterceptorInfo> has_side_effects;
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Value, has_side_effects);
  f(index, v8::Utils::ToLocal(value), callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallIndexedDefiner(
    Handle<InterceptorInfo> interceptor, uint32_t index,
    const v8::PropertyDescriptor& desc) {
  DCHECK(!interceptor->is_named());
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexedDefinerCallback);
  IndexedPropertyDefinerCallback f =
      ToCData<IndexedPropertyDefinerCallback>(interceptor->definer());
  Handle<InterceptorInfo> has_side_effects;
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Value, has_side_effects);
  f(index, desc, callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallIndexedDeleter(
    Handle<InterceptorInfo> interceptor, uint32_t index) {
  DCHECK(!interceptor->is_named());
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexedDeleterCallback);
  IndexedPropertyDeleterCallback f =
      ToCData<IndexedPropertyDeleterCallback>(interceptor->deleter());
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Boolean, interceptor);
  f(index, callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<JSObject> PropertyCallbackArguments::CallPropertyEnumerator(
    Handle<InterceptorInfo> interceptor) {
  // For now there is a single enumerator for indexed and named properties.
  IndexedPropertyEnumeratorCallback f =
      v8::ToCData<IndexedPropertyEnumeratorCallback>(interceptor->enumerator());
  // TODO(cbruni): assert same type for indexed and named callback.
  Isolate* isolate = this->isolate();
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Array, interceptor);
  f(callback_info);
  return GetReturnValue<JSObject>(isolate);
}

// -------------------------------------------------------------------------
// Accessors

Handle<Object> PropertyCallbackArguments::CallAccessorGetter(
    Handle<AccessorInfo> info, Handle<Name> name) {
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kAccessorGetterCallback);
  // Unlike interceptor callbacks we know that the property exists, so
  // the callback is allowed to have side effects.
  AcceptSideEffects();

  AccessorNameGetterCallback f =
      reinterpret_cast<AccessorNameGetterCallback>(info->getter(isolate));
  PREPARE_CALLBACK_INFO_ACCESSOR(isolate, f, v8::Value, info,
                                 handle(receiver(), isolate), ACCESSOR_GETTER);
  f(v8::Utils::ToLocal(name), callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallAccessorSetter(
    Handle<AccessorInfo> accessor_info, Handle<Name> name,
    Handle<Object> value) {
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kAccessorSetterCallback);
  // Unlike interceptor callbacks we know that the property exists, so
  // the callback is allowed to have side effects.
  AcceptSideEffects();

  AccessorNameSetterCallback f = reinterpret_cast<AccessorNameSetterCallback>(
      accessor_info->setter(isolate));
  PREPARE_CALLBACK_INFO_ACCESSOR(isolate, f, void, accessor_info,
                                 handle(receiver(), isolate), ACCESSOR_SETTER);
  f(v8::Utils::ToLocal(name), v8::Utils::ToLocal(value), callback_info);
  return GetReturnValue<Object>(isolate);
}

#undef PREPARE_CALLBACK_INFO_ACCESSOR
#undef PREPARE_CALLBACK_INFO_INTERCEPTOR

}  // namespace internal
}  // namespace v8

#endif  // V8_API_API_ARGUMENTS_INL_H_
