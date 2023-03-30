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
#include "v8-isolate.h"

namespace v8 {
namespace internal {

#if DEBUG
bool Object::IsApiCallResultType() const {
  if (IsSmi()) return true;
  DCHECK(IsHeapObject());
  return (IsString() || IsSymbol() || IsJSReceiver() || IsHeapNumber() ||
          IsBigInt() || IsUndefined() || IsTrue() || IsFalse() || IsNull());
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
  Object raw_object = *slot;
  if (raw_object.IsTheHole(isolate)) return Handle<V>();
  DCHECK(raw_object.IsApiCallResultType());
  return Handle<V>::cast(Handle<Object>(slot.location()));
}

inline JSObject PropertyCallbackArguments::holder() const {
  return JSObject::cast(*slot_at(T::kHolderIndex));
}

inline Object PropertyCallbackArguments::receiver() const {
  return *slot_at(T::kThisIndex);
}

inline JSReceiver FunctionCallbackArguments::holder() const {
  return JSReceiver::cast(*slot_at(T::kHolderIndex));
}

#define FOR_EACH_CALLBACK(F)                        \
  F(Query, query, Object, v8::Integer, interceptor) \
  F(Deleter, deleter, Object, v8::Boolean, Handle<Object>())

#define DCHECK_NAME_COMPATIBLE(interceptor, name) \
  DCHECK(interceptor->is_named());                \
  DCHECK(!name->IsPrivate());                     \
  DCHECK_IMPLIES(name->IsSymbol(), interceptor->can_intercept_symbols());

#define PREPARE_CALLBACK_INFO(ISOLATE, F, RETURN_VALUE, API_RETURN_TYPE, \
                              CALLBACK_INFO, RECEIVER, ACCESSOR_KIND)    \
  if (ISOLATE->debug_execution_mode() == DebugInfo::kSideEffects &&      \
      !ISOLATE->debug()->PerformSideEffectCheckForCallback(              \
          CALLBACK_INFO, RECEIVER, Debug::k##ACCESSOR_KIND)) {           \
    return RETURN_VALUE();                                               \
  }                                                                      \
  ExternalCallbackScope call_scope(ISOLATE, FUNCTION_ADDR(F));           \
  PropertyCallbackInfo<API_RETURN_TYPE> callback_info(values_);

#define PREPARE_CALLBACK_INFO_FAIL_SIDE_EFFECT_CHECK(ISOLATE, F, RETURN_VALUE, \
                                                     API_RETURN_TYPE)          \
  if (ISOLATE->debug_execution_mode() == DebugInfo::kSideEffects) {            \
    return RETURN_VALUE();                                                     \
  }                                                                            \
  ExternalCallbackScope call_scope(ISOLATE, FUNCTION_ADDR(F));                 \
  PropertyCallbackInfo<API_RETURN_TYPE> callback_info(values_);

#define CREATE_NAMED_CALLBACK(FUNCTION, TYPE, RETURN_TYPE, API_RETURN_TYPE, \
                              INFO_FOR_SIDE_EFFECT)                         \
  Handle<RETURN_TYPE> PropertyCallbackArguments::CallNamed##FUNCTION(       \
      Handle<InterceptorInfo> interceptor, Handle<Name> name) {             \
    DCHECK_NAME_COMPATIBLE(interceptor, name);                              \
    Isolate* isolate = this->isolate();                                     \
    RCS_SCOPE(isolate, RuntimeCallCounterId::kNamed##FUNCTION##Callback);   \
    Handle<Object> receiver_check_unsupported;                              \
    GenericNamedProperty##FUNCTION##Callback f =                            \
        ToCData<GenericNamedProperty##FUNCTION##Callback>(                  \
            interceptor->TYPE());                                           \
    PREPARE_CALLBACK_INFO(isolate, f, Handle<RETURN_TYPE>, API_RETURN_TYPE, \
                          INFO_FOR_SIDE_EFFECT, receiver_check_unsupported, \
                          NotAccessor);                                     \
    f(v8::Utils::ToLocal(name), callback_info);                             \
    return GetReturnValue<RETURN_TYPE>(isolate);                            \
  }

FOR_EACH_CALLBACK(CREATE_NAMED_CALLBACK)
#undef CREATE_NAMED_CALLBACK

#define CREATE_INDEXED_CALLBACK(FUNCTION, TYPE, RETURN_TYPE, API_RETURN_TYPE, \
                                INFO_FOR_SIDE_EFFECT)                         \
  Handle<RETURN_TYPE> PropertyCallbackArguments::CallIndexed##FUNCTION(       \
      Handle<InterceptorInfo> interceptor, uint32_t index) {                  \
    DCHECK(!interceptor->is_named());                                         \
    Isolate* isolate = this->isolate();                                       \
    RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexed##FUNCTION##Callback);   \
    Handle<Object> receiver_check_unsupported;                                \
    IndexedProperty##FUNCTION##Callback f =                                   \
        ToCData<IndexedProperty##FUNCTION##Callback>(interceptor->TYPE());    \
    PREPARE_CALLBACK_INFO(isolate, f, Handle<RETURN_TYPE>, API_RETURN_TYPE,   \
                          INFO_FOR_SIDE_EFFECT, receiver_check_unsupported,   \
                          NotAccessor);                                       \
    f(index, callback_info);                                                  \
    return GetReturnValue<RETURN_TYPE>(isolate);                              \
  }

FOR_EACH_CALLBACK(CREATE_INDEXED_CALLBACK)

#undef FOR_EACH_CALLBACK
#undef CREATE_INDEXED_CALLBACK

Handle<Object> FunctionCallbackArguments::Call(CallHandlerInfo handler) {
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kFunctionCallback);
  v8::FunctionCallback f =
      reinterpret_cast<v8::FunctionCallback>(handler.callback());
  Handle<Object> receiver_check_unsupported;
  if (isolate->debug_execution_mode() == DebugInfo::kSideEffects &&
      !isolate->debug()->PerformSideEffectCheckForCallback(
          handle(handler, isolate), receiver_check_unsupported,
          Debug::kNotAccessor)) {
    return Handle<Object>();
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

Handle<JSObject> PropertyCallbackArguments::CallNamedEnumerator(
    Handle<InterceptorInfo> interceptor) {
  DCHECK(interceptor->is_named());
  RCS_SCOPE(isolate(), RuntimeCallCounterId::kNamedEnumeratorCallback);
  return CallPropertyEnumerator(interceptor);
}

Handle<JSObject> PropertyCallbackArguments::CallIndexedEnumerator(
    Handle<InterceptorInfo> interceptor) {
  DCHECK(!interceptor->is_named());
  RCS_SCOPE(isolate(), RuntimeCallCounterId::kIndexedEnumeratorCallback);
  return CallPropertyEnumerator(interceptor);
}

Handle<Object> PropertyCallbackArguments::CallNamedGetter(
    Handle<InterceptorInfo> interceptor, Handle<Name> name) {
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  RCS_SCOPE(isolate(), RuntimeCallCounterId::kNamedGetterCallback);
  GenericNamedPropertyGetterCallback f =
      ToCData<GenericNamedPropertyGetterCallback>(interceptor->getter());
  return BasicCallNamedGetterCallback(f, name, interceptor);
}

Handle<Object> PropertyCallbackArguments::CallNamedDescriptor(
    Handle<InterceptorInfo> interceptor, Handle<Name> name) {
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  RCS_SCOPE(isolate(), RuntimeCallCounterId::kNamedDescriptorCallback);
  GenericNamedPropertyDescriptorCallback f =
      ToCData<GenericNamedPropertyDescriptorCallback>(
          interceptor->descriptor());
  return BasicCallNamedGetterCallback(f, name, interceptor);
}

Handle<Object> PropertyCallbackArguments::BasicCallNamedGetterCallback(
    GenericNamedPropertyGetterCallback f, Handle<Name> name,
    Handle<Object> info, Handle<Object> receiver) {
  DCHECK(!name->IsPrivate());
  Isolate* isolate = this->isolate();
  PREPARE_CALLBACK_INFO(isolate, f, Handle<Object>, v8::Value, info, receiver,
                        Getter);
  f(v8::Utils::ToLocal(name), callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallNamedSetter(
    Handle<InterceptorInfo> interceptor, Handle<Name> name,
    Handle<Object> value) {
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  GenericNamedPropertySetterCallback f =
      ToCData<GenericNamedPropertySetterCallback>(interceptor->setter());
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedSetterCallback);
  PREPARE_CALLBACK_INFO_FAIL_SIDE_EFFECT_CHECK(isolate, f, Handle<Object>,
                                               v8::Value);
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
  PREPARE_CALLBACK_INFO_FAIL_SIDE_EFFECT_CHECK(isolate, f, Handle<Object>,
                                               v8::Value);
  f(v8::Utils::ToLocal(name), desc, callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallIndexedSetter(
    Handle<InterceptorInfo> interceptor, uint32_t index, Handle<Object> value) {
  DCHECK(!interceptor->is_named());
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexedSetterCallback);
  IndexedPropertySetterCallback f =
      ToCData<IndexedPropertySetterCallback>(interceptor->setter());
  PREPARE_CALLBACK_INFO_FAIL_SIDE_EFFECT_CHECK(isolate, f, Handle<Object>,
                                               v8::Value);
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
  PREPARE_CALLBACK_INFO_FAIL_SIDE_EFFECT_CHECK(isolate, f, Handle<Object>,
                                               v8::Value);
  f(index, desc, callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallIndexedGetter(
    Handle<InterceptorInfo> interceptor, uint32_t index) {
  DCHECK(!interceptor->is_named());
  RCS_SCOPE(isolate(), RuntimeCallCounterId::kNamedGetterCallback);
  IndexedPropertyGetterCallback f =
      ToCData<IndexedPropertyGetterCallback>(interceptor->getter());
  return BasicCallIndexedGetterCallback(f, index, interceptor);
}

Handle<Object> PropertyCallbackArguments::CallIndexedDescriptor(
    Handle<InterceptorInfo> interceptor, uint32_t index) {
  DCHECK(!interceptor->is_named());
  RCS_SCOPE(isolate(), RuntimeCallCounterId::kIndexedDescriptorCallback);
  IndexedPropertyDescriptorCallback f =
      ToCData<IndexedPropertyDescriptorCallback>(interceptor->descriptor());
  return BasicCallIndexedGetterCallback(f, index, interceptor);
}

Handle<Object> PropertyCallbackArguments::BasicCallIndexedGetterCallback(
    IndexedPropertyGetterCallback f, uint32_t index, Handle<Object> info) {
  Isolate* isolate = this->isolate();
  Handle<Object> receiver_check_unsupported;
  PREPARE_CALLBACK_INFO(isolate, f, Handle<Object>, v8::Value, info,
                        receiver_check_unsupported, Getter);
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
  Handle<Object> receiver_check_unsupported;
  PREPARE_CALLBACK_INFO(isolate, f, Handle<JSObject>, v8::Array, interceptor,
                        receiver_check_unsupported, NotAccessor);
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
      reinterpret_cast<AccessorNameGetterCallback>(info->getter());
  return BasicCallNamedGetterCallback(f, name, info,
                                      handle(receiver(), isolate));
}

Handle<Object> PropertyCallbackArguments::CallAccessorSetter(
    Handle<AccessorInfo> accessor_info, Handle<Name> name,
    Handle<Object> value) {
  Isolate* isolate = this->isolate();
  RCS_SCOPE(isolate, RuntimeCallCounterId::kAccessorSetterCallback);
  // Unlike interceptor callbacks we know that the property exists, so
  // the callback is allowed to have side effects.
  AcceptSideEffects();

  AccessorNameSetterCallback f =
      reinterpret_cast<AccessorNameSetterCallback>(accessor_info->setter());
  PREPARE_CALLBACK_INFO(isolate, f, Handle<Object>, void, accessor_info,
                        handle(receiver(), isolate), Setter);
  f(v8::Utils::ToLocal(name), v8::Utils::ToLocal(value), callback_info);
  return GetReturnValue<Object>(isolate);
}

#undef PREPARE_CALLBACK_INFO
#undef PREPARE_CALLBACK_INFO_FAIL_SIDE_EFFECT_CHECK

}  // namespace internal
}  // namespace v8

#endif  // V8_API_API_ARGUMENTS_INL_H_
