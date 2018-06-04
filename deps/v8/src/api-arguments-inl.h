// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_ARGUMENTS_INL_H_
#define V8_API_ARGUMENTS_INL_H_

#include "src/api-arguments.h"

#include "src/tracing/trace-event.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

#define FOR_EACH_CALLBACK(F)                        \
  F(Query, query, Object, v8::Integer, interceptor) \
  F(Deleter, deleter, Object, v8::Boolean, Handle<Object>())

#define DCHECK_NAME_COMPATIBLE(interceptor, name) \
  DCHECK(interceptor->is_named());                \
  DCHECK(!name->IsPrivate());                     \
  DCHECK_IMPLIES(name->IsSymbol(), interceptor->can_intercept_symbols());

#define PREPARE_CALLBACK_INFO(ISOLATE, F, RETURN_VALUE, API_RETURN_TYPE,     \
                              CALLBACK_INFO)                                 \
  if (ISOLATE->debug_execution_mode() == DebugInfo::kSideEffects &&          \
      !ISOLATE->debug()->PerformSideEffectCheckForCallback(CALLBACK_INFO)) { \
    return RETURN_VALUE();                                                   \
  }                                                                          \
  VMState<EXTERNAL> state(ISOLATE);                                          \
  ExternalCallbackScope call_scope(ISOLATE, FUNCTION_ADDR(F));               \
  PropertyCallbackInfo<API_RETURN_TYPE> callback_info(begin());

#define CREATE_NAMED_CALLBACK(FUNCTION, TYPE, RETURN_TYPE, API_RETURN_TYPE,   \
                              INFO_FOR_SIDE_EFFECT)                           \
  Handle<RETURN_TYPE> PropertyCallbackArguments::CallNamed##FUNCTION(         \
      Handle<InterceptorInfo> interceptor, Handle<Name> name) {               \
    DCHECK_NAME_COMPATIBLE(interceptor, name);                                \
    Isolate* isolate = this->isolate();                                       \
    RuntimeCallTimerScope timer(                                              \
        isolate, RuntimeCallCounterId::kNamed##FUNCTION##Callback);           \
    GenericNamedProperty##FUNCTION##Callback f =                              \
        ToCData<GenericNamedProperty##FUNCTION##Callback>(                    \
            interceptor->TYPE());                                             \
    PREPARE_CALLBACK_INFO(isolate, f, Handle<RETURN_TYPE>, API_RETURN_TYPE,   \
                          INFO_FOR_SIDE_EFFECT);                              \
    LOG(isolate,                                                              \
        ApiNamedPropertyAccess("interceptor-named-" #TYPE, holder(), *name)); \
    f(v8::Utils::ToLocal(name), callback_info);                               \
    return GetReturnValue<RETURN_TYPE>(isolate);                              \
  }

FOR_EACH_CALLBACK(CREATE_NAMED_CALLBACK)
#undef CREATE_NAMED_CALLBACK

#define CREATE_INDEXED_CALLBACK(FUNCTION, TYPE, RETURN_TYPE, API_RETURN_TYPE, \
                                INFO_FOR_SIDE_EFFECT)                         \
  Handle<RETURN_TYPE> PropertyCallbackArguments::CallIndexed##FUNCTION(       \
      Handle<InterceptorInfo> interceptor, uint32_t index) {                  \
    DCHECK(!interceptor->is_named());                                         \
    Isolate* isolate = this->isolate();                                       \
    RuntimeCallTimerScope timer(                                              \
        isolate, RuntimeCallCounterId::kIndexed##FUNCTION##Callback);         \
    IndexedProperty##FUNCTION##Callback f =                                   \
        ToCData<IndexedProperty##FUNCTION##Callback>(interceptor->TYPE());    \
    PREPARE_CALLBACK_INFO(isolate, f, Handle<RETURN_TYPE>, API_RETURN_TYPE,   \
                          INFO_FOR_SIDE_EFFECT);                              \
    LOG(isolate, ApiIndexedPropertyAccess("interceptor-indexed-" #TYPE,       \
                                          holder(), index));                  \
    f(index, callback_info);                                                  \
    return GetReturnValue<RETURN_TYPE>(isolate);                              \
  }

FOR_EACH_CALLBACK(CREATE_INDEXED_CALLBACK)

#undef FOR_EACH_CALLBACK
#undef CREATE_INDEXED_CALLBACK

Handle<Object> FunctionCallbackArguments::Call(CallHandlerInfo* handler) {
  Isolate* isolate = this->isolate();
  LOG(isolate, ApiObjectAccess("call", holder()));
  RuntimeCallTimerScope timer(isolate, RuntimeCallCounterId::kFunctionCallback);
  v8::FunctionCallback f =
      v8::ToCData<v8::FunctionCallback>(handler->callback());
  if (isolate->debug_execution_mode() == DebugInfo::kSideEffects &&
      !isolate->debug()->PerformSideEffectCheckForCallback(handle(handler))) {
    return Handle<Object>();
  }
  VMState<EXTERNAL> state(isolate);
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));
  FunctionCallbackInfo<v8::Value> info(begin(), argv_, argc_);
  f(info);
  return GetReturnValue<Object>(isolate);
}

Handle<JSObject> PropertyCallbackArguments::CallNamedEnumerator(
    Handle<InterceptorInfo> interceptor) {
  DCHECK(interceptor->is_named());
  LOG(isolate(), ApiObjectAccess("interceptor-named-enumerator", holder()));
  RuntimeCallTimerScope timer(isolate(),
                              RuntimeCallCounterId::kNamedEnumeratorCallback);
  return CallPropertyEnumerator(interceptor);
}

Handle<JSObject> PropertyCallbackArguments::CallIndexedEnumerator(
    Handle<InterceptorInfo> interceptor) {
  DCHECK(!interceptor->is_named());
  LOG(isolate(), ApiObjectAccess("interceptor-indexed-enumerator", holder()));
  RuntimeCallTimerScope timer(isolate(),
                              RuntimeCallCounterId::kIndexedEnumeratorCallback);
  return CallPropertyEnumerator(interceptor);
}

Handle<Object> PropertyCallbackArguments::CallNamedGetter(
    Handle<InterceptorInfo> interceptor, Handle<Name> name) {
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  Isolate* isolate = this->isolate();
  RuntimeCallTimerScope timer(isolate,
                              RuntimeCallCounterId::kNamedGetterCallback);
  LOG(isolate,
      ApiNamedPropertyAccess("interceptor-named-getter", holder(), *name));
  GenericNamedPropertyGetterCallback f =
      ToCData<GenericNamedPropertyGetterCallback>(interceptor->getter());
  return BasicCallNamedGetterCallback(f, name, interceptor);
}

Handle<Object> PropertyCallbackArguments::CallNamedDescriptor(
    Handle<InterceptorInfo> interceptor, Handle<Name> name) {
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  Isolate* isolate = this->isolate();
  RuntimeCallTimerScope timer(isolate,
                              RuntimeCallCounterId::kNamedDescriptorCallback);
  LOG(isolate,
      ApiNamedPropertyAccess("interceptor-named-descriptor", holder(), *name));
  GenericNamedPropertyDescriptorCallback f =
      ToCData<GenericNamedPropertyDescriptorCallback>(
          interceptor->descriptor());
  return BasicCallNamedGetterCallback(f, name, interceptor);
}

Handle<Object> PropertyCallbackArguments::BasicCallNamedGetterCallback(
    GenericNamedPropertyGetterCallback f, Handle<Name> name,
    Handle<Object> info) {
  DCHECK(!name->IsPrivate());
  Isolate* isolate = this->isolate();
  PREPARE_CALLBACK_INFO(isolate, f, Handle<Object>, v8::Value, info);
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
  RuntimeCallTimerScope timer(isolate,
                              RuntimeCallCounterId::kNamedSetterCallback);
  Handle<Object> side_effect_check_not_supported;
  PREPARE_CALLBACK_INFO(isolate, f, Handle<Object>, v8::Value,
                        side_effect_check_not_supported);
  LOG(isolate,
      ApiNamedPropertyAccess("interceptor-named-set", holder(), *name));
  f(v8::Utils::ToLocal(name), v8::Utils::ToLocal(value), callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallNamedDefiner(
    Handle<InterceptorInfo> interceptor, Handle<Name> name,
    const v8::PropertyDescriptor& desc) {
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  Isolate* isolate = this->isolate();
  RuntimeCallTimerScope timer(isolate,
                              RuntimeCallCounterId::kNamedDefinerCallback);
  GenericNamedPropertyDefinerCallback f =
      ToCData<GenericNamedPropertyDefinerCallback>(interceptor->definer());
  Handle<Object> side_effect_check_not_supported;
  PREPARE_CALLBACK_INFO(isolate, f, Handle<Object>, v8::Value,
                        side_effect_check_not_supported);
  LOG(isolate,
      ApiNamedPropertyAccess("interceptor-named-define", holder(), *name));
  f(v8::Utils::ToLocal(name), desc, callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallIndexedSetter(
    Handle<InterceptorInfo> interceptor, uint32_t index, Handle<Object> value) {
  DCHECK(!interceptor->is_named());
  Isolate* isolate = this->isolate();
  RuntimeCallTimerScope timer(isolate,
                              RuntimeCallCounterId::kIndexedSetterCallback);
  IndexedPropertySetterCallback f =
      ToCData<IndexedPropertySetterCallback>(interceptor->setter());
  Handle<Object> side_effect_check_not_supported;
  PREPARE_CALLBACK_INFO(isolate, f, Handle<Object>, v8::Value,
                        side_effect_check_not_supported);
  LOG(isolate,
      ApiIndexedPropertyAccess("interceptor-indexed-set", holder(), index));
  f(index, v8::Utils::ToLocal(value), callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallIndexedDefiner(
    Handle<InterceptorInfo> interceptor, uint32_t index,
    const v8::PropertyDescriptor& desc) {
  DCHECK(!interceptor->is_named());
  Isolate* isolate = this->isolate();
  RuntimeCallTimerScope timer(isolate,
                              RuntimeCallCounterId::kIndexedDefinerCallback);
  IndexedPropertyDefinerCallback f =
      ToCData<IndexedPropertyDefinerCallback>(interceptor->definer());
  Handle<Object> side_effect_check_not_supported;
  PREPARE_CALLBACK_INFO(isolate, f, Handle<Object>, v8::Value,
                        side_effect_check_not_supported);
  LOG(isolate,
      ApiIndexedPropertyAccess("interceptor-indexed-define", holder(), index));
  f(index, desc, callback_info);
  return GetReturnValue<Object>(isolate);
}

Handle<Object> PropertyCallbackArguments::CallIndexedGetter(
    Handle<InterceptorInfo> interceptor, uint32_t index) {
  DCHECK(!interceptor->is_named());
  Isolate* isolate = this->isolate();
  RuntimeCallTimerScope timer(isolate,
                              RuntimeCallCounterId::kNamedGetterCallback);
  LOG(isolate,
      ApiIndexedPropertyAccess("interceptor-indexed-getter", holder(), index));
  IndexedPropertyGetterCallback f =
      ToCData<IndexedPropertyGetterCallback>(interceptor->getter());
  return BasicCallIndexedGetterCallback(f, index, interceptor);
}

Handle<Object> PropertyCallbackArguments::CallIndexedDescriptor(
    Handle<InterceptorInfo> interceptor, uint32_t index) {
  DCHECK(!interceptor->is_named());
  Isolate* isolate = this->isolate();
  RuntimeCallTimerScope timer(isolate,
                              RuntimeCallCounterId::kIndexedDescriptorCallback);
  LOG(isolate, ApiIndexedPropertyAccess("interceptor-indexed-descriptor",
                                        holder(), index));
  IndexedPropertyDescriptorCallback f =
      ToCData<IndexedPropertyDescriptorCallback>(interceptor->descriptor());
  return BasicCallIndexedGetterCallback(f, index, interceptor);
}

Handle<Object> PropertyCallbackArguments::BasicCallIndexedGetterCallback(
    IndexedPropertyGetterCallback f, uint32_t index, Handle<Object> info) {
  Isolate* isolate = this->isolate();
  PREPARE_CALLBACK_INFO(isolate, f, Handle<Object>, v8::Value, info);
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
  PREPARE_CALLBACK_INFO(isolate, f, Handle<JSObject>, v8::Array, interceptor);
  f(callback_info);
  return GetReturnValue<JSObject>(isolate);
}

// -------------------------------------------------------------------------
// Accessors

Handle<Object> PropertyCallbackArguments::CallAccessorGetter(
    Handle<AccessorInfo> info, Handle<Name> name) {
  Isolate* isolate = this->isolate();
  RuntimeCallTimerScope timer(isolate,
                              RuntimeCallCounterId::kAccessorGetterCallback);
  LOG(isolate, ApiNamedPropertyAccess("accessor-getter", holder(), *name));
  AccessorNameGetterCallback f =
      ToCData<AccessorNameGetterCallback>(info->getter());
  return BasicCallNamedGetterCallback(f, name, info);
}

Handle<Object> PropertyCallbackArguments::CallAccessorSetter(
    Handle<AccessorInfo> accessor_info, Handle<Name> name,
    Handle<Object> value) {
  Isolate* isolate = this->isolate();
  RuntimeCallTimerScope timer(isolate,
                              RuntimeCallCounterId::kAccessorSetterCallback);
  AccessorNameSetterCallback f =
      ToCData<AccessorNameSetterCallback>(accessor_info->setter());
  Handle<Object> side_effect_check_not_supported;
  PREPARE_CALLBACK_INFO(isolate, f, Handle<Object>, void,
                        side_effect_check_not_supported);
  LOG(isolate, ApiNamedPropertyAccess("accessor-setter", holder(), *name));
  f(v8::Utils::ToLocal(name), v8::Utils::ToLocal(value), callback_info);
  return GetReturnValue<Object>(isolate);
}

#undef PREPARE_CALLBACK_INFO

}  // namespace internal
}  // namespace v8

#endif  // V8_API_ARGUMENTS_INL_H_
