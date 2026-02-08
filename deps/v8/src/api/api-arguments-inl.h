// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_API_ARGUMENTS_INL_H_
#define V8_API_API_ARGUMENTS_INL_H_

#include "src/api/api-arguments.h"
// Include the non-inl header before the rest of the headers.

#include "src/api/api-inl.h"
#include "src/debug/debug.h"
#include "src/execution/vm-state-inl.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/instance-type.h"
#include "src/objects/slots-inl.h"

namespace v8 {
namespace internal {

template <typename T>
Handle<T> PropertyCallbackArguments::GetReturnValue() const {
  // Check the ReturnValue.
  FullObjectSlot slot = slot_at(kReturnValueIndex);
  DCHECK(Is<JSAny>(*slot));
  return Cast<T>(Handle<Object>(slot.location()));
}

bool PropertyCallbackArguments::is_named() const {
  int frame_type = Smi::ToInt(Tagged<Object>(values_[T::kFrameTypeIndex]));
  DCHECK(frame_type == StackFrame::API_NAMED_ACCESSOR_EXIT ||
         frame_type == StackFrame::API_INDEXED_ACCESSOR_EXIT);
  return frame_type == StackFrame::API_NAMED_ACCESSOR_EXIT;
}

void PropertyCallbackArguments::set_property_key(Tagged<Name> name) {
  values_[T::kPropertyKeyIndex] = name->ptr();
  values_[T::kFrameTypeIndex] =
      Smi::FromInt(StackFrame::API_NAMED_ACCESSOR_EXIT).ptr();
}

void PropertyCallbackArguments::set_property_key(uint32_t index) {
  values_[T::kPropertyKeyIndex] = index;
  values_[T::kFrameTypeIndex] =
      Smi::FromInt(StackFrame::API_INDEXED_ACCESSOR_EXIT).ptr();
}

DirectHandle<JSObject> PropertyCallbackArguments::holder() const {
  return DirectHandle<JSObject>::FromSlot(slot_at(T::kHolderIndex).location());
}

DirectHandle<Object> PropertyCallbackArguments::receiver() const {
  return DirectHandle<Object>::FromSlot(slot_at(T::kThisIndex).location());
}

#define DCHECK_NAME_COMPATIBLE(interceptor, name) \
  DCHECK(interceptor->is_named());                \
  DCHECK(!name->IsAnyPrivate());                  \
  DCHECK_IMPLIES(IsSymbol(*name), interceptor->can_intercept_symbols());

#define PREPARE_CALLBACK_INFO_ACCESSOR(ISOLATE, F, API_RETURN_TYPE,            \
                                       ACCESSOR_INFO, RECEIVER, ACCESSOR_KIND, \
                                       EXCEPTION_CONTEXT)                      \
  if (ISOLATE->should_check_side_effects() &&                                  \
      !ISOLATE->debug()->PerformSideEffectCheckForAccessor(                    \
          ACCESSOR_INFO, RECEIVER, ACCESSOR_KIND)) {                           \
    return {};                                                                 \
  }                                                                            \
  const PropertyCallbackInfo<API_RETURN_TYPE>& callback_info =                 \
      GetPropertyCallbackInfo<API_RETURN_TYPE>();                              \
  ExternalCallbackScope call_scope(ISOLATE, FUNCTION_ADDR(F),                  \
                                   EXCEPTION_CONTEXT, &callback_info);

#define PREPARE_CALLBACK_INFO_INTERCEPTOR(ISOLATE, F, API_RETURN_TYPE,         \
                                          INTERCEPTOR_INFO, EXCEPTION_CONTEXT) \
  if (ISOLATE->should_check_side_effects() &&                                  \
      !ISOLATE->debug()->PerformSideEffectCheckForInterceptor(                 \
          INTERCEPTOR_INFO)) {                                                 \
    return {};                                                                 \
  }                                                                            \
  const PropertyCallbackInfo<API_RETURN_TYPE>& callback_info =                 \
      GetPropertyCallbackInfo<API_RETURN_TYPE>();                              \
  ExternalCallbackScope call_scope(ISOLATE, FUNCTION_ADDR(F),                  \
                                   EXCEPTION_CONTEXT, &callback_info);

template <typename ArgT>
FunctionCallbackArguments::FunctionCallbackArguments(
    Isolate* isolate, Tagged<FunctionTemplateInfo> target,
    Tagged<Object> receiver, const base::Vector<const ArgT> args)
    : Relocatable(isolate) {
  Initialize<false>(isolate, target, Smi::zero(), receiver, args);
}

template <typename ArgT>
FunctionCallbackArguments::FunctionCallbackArguments(
    Isolate* isolate, Tagged<FunctionTemplateInfo> target,
    Tagged<HeapObject> new_target, Tagged<Object> receiver,
    const base::Vector<const ArgT> args)
    : Relocatable(isolate) {
  Initialize<true>(isolate, target, new_target, receiver, args);
}

template <bool is_construct, typename ArgT>
  requires(std::is_same_v<ArgT, DirectHandle<Object>> ||
           std::is_same_v<ArgT, Address>)
void FunctionCallbackArguments::Initialize(
    Isolate* isolate, Tagged<FunctionTemplateInfo> target,
    Tagged<Object> new_target, Tagged<Object> receiver,
    const base::Vector<const ArgT> args) {
  uint32_t argc = static_cast<uint32_t>(args.size());
  values_.resize(argc + T::kArgsLength + T::kOptionalArgsLength);

  Address* values = &values_.data()[T::kOptionalArgsLength];

  // Initialize frame part.
  values[T::kNewTargetIndex] = is_construct ? new_target.ptr() : 0;
  values[T::kArgcIndex] = argc;
  values[T::kFrameTypeIndex] =
      Smi::FromInt(is_construct ? StackFrame::API_CONSTRUCT_EXIT
                                : StackFrame::API_CALLBACK_EXIT)
          .ptr();

  if (DEBUG_BOOL) {
    // These values are not supposed to be looked at.
    values[T::kFrameSPIndex] = kZapValue;
    values[T::kFrameConstantPoolIndex] = kZapValue;
    values[T::kFrameFPIndex] = kZapValue;
    values[T::kFramePCIndex] = kZapValue;
  }

  // Initialize Api arguments part.
  values[T::kTargetIndex] = target.ptr();
  values[T::kIsolateIndex] = reinterpret_cast<Address>(isolate);
  values[T::kReturnValueIndex] = ReadOnlyRoots(isolate).undefined_value().ptr();
  values[T::kContextIndex] = isolate->context().ptr();

  // Make sure the Isolate slot is safe to visit by GC (Isolate pointer
  // is guaranteed to be page aligned).
  DCHECK(HAS_SMI_TAG(values[T::kIsolateIndex]));

  // Initialize JS arguments part.
  values[T::kReceiverIndex] = receiver.ptr();
  for (uint32_t i = 0; i < argc; ++i) {
    if constexpr (std::is_same_v<ArgT, Address>) {
      values[T::kFirstJSArgumentIndex + i] = args[i];
    } else {
      values[T::kFirstJSArgumentIndex + i] = (*args[i]).ptr();
    }
  }
}

FunctionCallbackArguments::~FunctionCallbackArguments() {
  if (DEBUG_BOOL) {
    // Make sure the result handle located inside this structure is not used
    // after this object dies.
    values_.data()[T::kReturnValueIndex] = kZapValue;
  }
}

Tagged<JSAny> FunctionCallbackArguments::CallOrConstruct(
    Isolate* isolate, Tagged<FunctionTemplateInfo> function,
    bool is_construct) {
  RCS_SCOPE(isolate, RuntimeCallCounterId::kFunctionCallback);
  v8::FunctionCallback f =
      reinterpret_cast<v8::FunctionCallback>(function->callback(isolate));
  if (isolate->should_check_side_effects() &&
      !isolate->debug()->PerformSideEffectCheckForCallback(
          handle(function, isolate))) {
    return {};
  }
  // v8::FunctionCallbackInfo structure might start at different positions in
  // values_ array depending on whether it's a construct call or not.
  auto info =
      reinterpret_cast<FunctionCallbackInfo<v8::Value>*>(slot_at(0).location());
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f),
                                   is_construct ? ExceptionContext::kConstructor
                                                : ExceptionContext::kOperation,
                                   info);
  f(*info);

  Tagged<Object> result = *slot_at(T::kReturnValueIndex);
  DCHECK(Is<JSAny>(result));
  return Cast<JSAny>(result);
}

PropertyCallbackArguments::PropertyCallbackArguments(Isolate* isolate,
                                                     Tagged<Object> receiver,
                                                     Tagged<JSObject> holder)
    : Relocatable(isolate)
#ifdef DEBUG
      ,
      is_setter_definer_deleter_(false),
      javascript_execution_counter_(isolate->javascript_execution_counter())
#endif  // DEBUG
{
  Initialize(isolate, receiver, holder);
}

PropertyCallbackArguments::PropertyCallbackArguments(
    Isolate* isolate, Tagged<Object> receiver, Tagged<JSObject> holder,
    Maybe<ShouldThrow> should_throw)
    : Relocatable(isolate)
#ifdef DEBUG
      ,
      is_setter_definer_deleter_(true),
      javascript_execution_counter_(isolate->javascript_execution_counter())
#endif  // DEBUG
{
  Initialize(isolate, receiver, holder);

  int value = Internals::kInferShouldThrowMode;
  if (should_throw.IsJust()) {
    value = should_throw.FromJust();
  }
  slot_at(T::kShouldThrowOnErrorIndex).store(Smi::FromInt(value));
}

void PropertyCallbackArguments::Initialize(Isolate* isolate,
                                           Tagged<Object> self,
                                           Tagged<JSObject> holder) {
  if (DEBUG_BOOL) {
    // Zap these fields to ensure that they are initialized by a subsequent
    // CallXXX(..).
    values_[T::kFrameSPIndex] = kZapValue;
    values_[T::kFrameConstantPoolIndex] = kZapValue;
    values_[T::kFrameTypeIndex] = kZapValue;
    values_[T::kFrameFPIndex] = kZapValue;
    values_[T::kFramePCIndex] = kZapValue;

    values_[T::kPropertyKeyIndex] = kZapValue;
    values_[T::kReturnValueIndex] = kZapValue;
    values_[T::kCallbackInfoIndex] = kZapValue;
    // This field is used only for setter/definer/deleter callbacks.
    values_[T::kShouldThrowOnErrorIndex] = kZapValue;
  }
  values_[T::kIsolateIndex] = reinterpret_cast<Address>(isolate);

  static_assert(T::kHolderIndex == T::kUnusedIndex ||
                T::kHolderIndex == (T::kUnusedIndex + 1));
  if (T::kHolderIndex != T::kUnusedIndex) {
    // If there's an unused slot, initialize it to zero to let GC safely
    // visit it.
    values_[T::kUnusedIndex] = 0;
  }
  values_[T::kHolderIndex] = holder.ptr();
  DCHECK(!IsJSGlobalObject(*holder));
  values_[T::kThisIndex] = self.ptr();

  // Make sure the Isolate slot is safe to visit by GC (Isolate pointer
  // is guaranteed to be page aligned).
  DCHECK(HAS_SMI_TAG(values_[T::kIsolateIndex]));
}

PropertyCallbackArguments::~PropertyCallbackArguments() {
#ifdef DEBUG
  // TODO(chromium:1310062): enable this check.
  // if (javascript_execution_counter_) {
  //   CHECK_WITH_MSG(javascript_execution_counter_ ==
  //                      isolate()->javascript_execution_counter(),
  //                  "Unexpected side effect detected");
  // }
  values_[T::kReturnValueIndex] = kZapValue;
#endif  // DEBUG
}

Maybe<InterceptorResult> PropertyCallbackArguments::GetBooleanReturnValue(
    Isolate* isolate, v8::Intercepted intercepted,
    const char* callback_kind_for_error_message, bool ignore_return_value) {
  if (isolate->has_exception()) {
    // TODO(ishell, 328490288): fix Node.js which has Setter/Definer
    // interceptor callbacks not returning v8::Intercepted::kYes on exceptions.
    if ((false) && DEBUG_BOOL && (intercepted == v8::Intercepted::kNo)) {
      FATAL(
          "Check failed: %s interceptor callback has thrown an "
          "exception but hasn't returned v8::Intercepted::kYes.",
          callback_kind_for_error_message);
    }
    return Nothing<InterceptorResult>();
  }

  if (intercepted == v8::Intercepted::kNo) {
    // Not intercepted, there must be no side effects including exceptions.
    DCHECK(!isolate->has_exception());
    return Just(InterceptorResult::kNotIntercepted);
  }
  DCHECK_EQ(intercepted, v8::Intercepted::kYes);
  AcceptSideEffects();

  if (ignore_return_value) return Just(InterceptorResult::kTrue);

  bool result = IsTrue(*GetReturnValue<Boolean>(), isolate);
  return Just(result ? InterceptorResult::kTrue : InterceptorResult::kFalse);
}

// -------------------------------------------------------------------------
// Named Interceptor callbacks.

DirectHandle<JSObjectOrUndefined>
PropertyCallbackArguments::CallNamedEnumerator(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor) {
  DCHECK(!is_setter_definer_deleter_);
  DCHECK(interceptor->is_named());
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedEnumeratorCallback);
  return CallPropertyEnumerator(isolate, interceptor);
}

// TODO(ishell): return std::optional<PropertyAttributes>.
DirectHandle<Object> PropertyCallbackArguments::CallNamedQuery(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
    DirectHandle<Name> name) {
  DCHECK(!is_setter_definer_deleter_);
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedQueryCallback);
  set_property_key(*name);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  slot_at(kReturnValueIndex).store(Smi::FromInt(v8::None));
  NamedPropertyQueryCallback f = reinterpret_cast<NamedPropertyQueryCallback>(
      interceptor->named_query(isolate));
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Integer, interceptor,
                                    ExceptionContext::kNamedQuery);
  v8::Intercepted intercepted = f(v8::Utils::ToLocal(name), callback_info);
  if (intercepted == v8::Intercepted::kNo) return {};
  return GetReturnValue<Object>();
}

DirectHandle<JSAny> PropertyCallbackArguments::CallNamedGetter(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
    DirectHandle<Name> name) {
  DCHECK(!is_setter_definer_deleter_);
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedGetterCallback);
  set_property_key(*name);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).undefined_value());
  NamedPropertyGetterCallback f = reinterpret_cast<NamedPropertyGetterCallback>(
      interceptor->named_getter(isolate));
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Value, interceptor,
                                    ExceptionContext::kNamedGetter);
  v8::Intercepted intercepted = f(v8::Utils::ToLocal(name), callback_info);
  if (intercepted == v8::Intercepted::kNo) return {};
  return GetReturnValue<JSAny>();
}

Handle<JSAny> PropertyCallbackArguments::CallNamedDescriptor(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
    DirectHandle<Name> name) {
  DCHECK(!is_setter_definer_deleter_);
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedDescriptorCallback);
  set_property_key(*name);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).undefined_value());
  NamedPropertyDescriptorCallback f =
      reinterpret_cast<NamedPropertyDescriptorCallback>(
          interceptor->named_descriptor(isolate));
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Value, interceptor,
                                    ExceptionContext::kNamedDescriptor);
  v8::Intercepted intercepted = f(v8::Utils::ToLocal(name), callback_info);
  if (intercepted == v8::Intercepted::kNo) return {};
  return GetReturnValue<JSAny>();
}

v8::Intercepted PropertyCallbackArguments::CallNamedSetter(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
    DirectHandle<Name> name, DirectHandle<Object> value) {
  DCHECK(is_setter_definer_deleter_);
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedSetterCallback);
  set_property_key(*name);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).true_value());
  NamedPropertySetterCallback f = reinterpret_cast<NamedPropertySetterCallback>(
      interceptor->named_setter(isolate));
  DirectHandle<InterceptorInfo> has_side_effects;
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, void, has_side_effects,
                                    ExceptionContext::kNamedSetter);
  v8::Intercepted intercepted =
      f(v8::Utils::ToLocal(name), v8::Utils::ToLocal(value), callback_info);
  return intercepted;
}

v8::Intercepted PropertyCallbackArguments::CallNamedDefiner(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
    DirectHandle<Name> name, const v8::PropertyDescriptor& desc) {
  DCHECK(is_setter_definer_deleter_);
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedDefinerCallback);
  set_property_key(*name);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).true_value());
  NamedPropertyDefinerCallback f =
      reinterpret_cast<NamedPropertyDefinerCallback>(
          interceptor->named_definer(isolate));
  DirectHandle<InterceptorInfo> has_side_effects;
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, void, has_side_effects,
                                    ExceptionContext::kNamedDefiner);
  v8::Intercepted intercepted =
      f(v8::Utils::ToLocal(name), desc, callback_info);
  return intercepted;
}

v8::Intercepted PropertyCallbackArguments::CallNamedDeleter(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
    DirectHandle<Name> name) {
  DCHECK(is_setter_definer_deleter_);
  DCHECK_NAME_COMPATIBLE(interceptor, name);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedDeleterCallback);
  set_property_key(*name);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).true_value());
  NamedPropertyDeleterCallback f =
      reinterpret_cast<NamedPropertyDeleterCallback>(
          interceptor->named_deleter(isolate));
  DirectHandle<InterceptorInfo> has_side_effects;
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Boolean, has_side_effects,
                                    ExceptionContext::kNamedDeleter);
  v8::Intercepted intercepted = f(v8::Utils::ToLocal(name), callback_info);
  return intercepted;
}

// -------------------------------------------------------------------------
// Indexed Interceptor callbacks.

DirectHandle<JSObjectOrUndefined>
PropertyCallbackArguments::CallIndexedEnumerator(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor) {
  DCHECK(!is_setter_definer_deleter_);
  DCHECK(!interceptor->is_named());
  RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexedEnumeratorCallback);
  return CallPropertyEnumerator(isolate, interceptor);
}

// TODO(ishell): return std::optional<PropertyAttributes>.
DirectHandle<Object> PropertyCallbackArguments::CallIndexedQuery(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
    uint32_t index) {
  DCHECK(!is_setter_definer_deleter_);
  DCHECK(!interceptor->is_named());
  RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexedQueryCallback);
  set_property_key(index);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  slot_at(kReturnValueIndex).store(Smi::FromInt(v8::None));
  IndexedPropertyQueryCallbackV2 f =
      reinterpret_cast<IndexedPropertyQueryCallbackV2>(
          interceptor->indexed_query(isolate));
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Integer, interceptor,
                                    ExceptionContext::kIndexedQuery);
  v8::Intercepted intercepted = f(index, callback_info);
  if (intercepted == v8::Intercepted::kNo) return {};
  return GetReturnValue<Object>();
}

DirectHandle<JSAny> PropertyCallbackArguments::CallIndexedGetter(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
    uint32_t index) {
  DCHECK(!is_setter_definer_deleter_);
  DCHECK(!interceptor->is_named());
  RCS_SCOPE(isolate, RuntimeCallCounterId::kNamedGetterCallback);
  set_property_key(index);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).undefined_value());
  IndexedPropertyGetterCallbackV2 f =
      reinterpret_cast<IndexedPropertyGetterCallbackV2>(
          interceptor->indexed_getter(isolate));
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Value, interceptor,
                                    ExceptionContext::kIndexedGetter);
  v8::Intercepted intercepted = f(index, callback_info);
  if (intercepted == v8::Intercepted::kNo) return {};
  return GetReturnValue<JSAny>();
}

Handle<JSAny> PropertyCallbackArguments::CallIndexedDescriptor(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
    uint32_t index) {
  DCHECK(!is_setter_definer_deleter_);
  DCHECK(!interceptor->is_named());
  RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexedDescriptorCallback);
  set_property_key(index);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).undefined_value());
  IndexedPropertyDescriptorCallbackV2 f =
      reinterpret_cast<IndexedPropertyDescriptorCallbackV2>(
          interceptor->indexed_descriptor(isolate));
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Value, interceptor,
                                    ExceptionContext::kIndexedDescriptor);
  v8::Intercepted intercepted = f(index, callback_info);
  if (intercepted == v8::Intercepted::kNo) return {};
  return GetReturnValue<JSAny>();
}

v8::Intercepted PropertyCallbackArguments::CallIndexedSetter(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor, uint32_t index,
    DirectHandle<Object> value) {
  DCHECK(is_setter_definer_deleter_);
  DCHECK(!interceptor->is_named());
  RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexedSetterCallback);
  set_property_key(index);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).true_value());
  IndexedPropertySetterCallbackV2 f =
      reinterpret_cast<IndexedPropertySetterCallbackV2>(
          interceptor->indexed_setter(isolate));
  DirectHandle<InterceptorInfo> has_side_effects;
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, void, has_side_effects,
                                    ExceptionContext::kIndexedSetter);
  v8::Intercepted intercepted =
      f(index, v8::Utils::ToLocal(value), callback_info);
  return intercepted;
}

v8::Intercepted PropertyCallbackArguments::CallIndexedDefiner(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor, uint32_t index,
    const v8::PropertyDescriptor& desc) {
  DCHECK(is_setter_definer_deleter_);
  DCHECK(!interceptor->is_named());
  RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexedDefinerCallback);
  set_property_key(index);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).true_value());
  IndexedPropertyDefinerCallbackV2 f =
      reinterpret_cast<IndexedPropertyDefinerCallbackV2>(
          interceptor->indexed_definer(isolate));
  DirectHandle<InterceptorInfo> has_side_effects;
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, void, has_side_effects,
                                    ExceptionContext::kIndexedDefiner);
  v8::Intercepted intercepted = f(index, desc, callback_info);
  return intercepted;
}

v8::Intercepted PropertyCallbackArguments::CallIndexedDeleter(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor,
    uint32_t index) {
  DCHECK(is_setter_definer_deleter_);
  DCHECK(!interceptor->is_named());
  RCS_SCOPE(isolate, RuntimeCallCounterId::kIndexedDeleterCallback);
  set_property_key(index);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).true_value());
  IndexedPropertyDeleterCallbackV2 f =
      reinterpret_cast<IndexedPropertyDeleterCallbackV2>(
          interceptor->indexed_deleter(isolate));
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Boolean, interceptor,
                                    ExceptionContext::kIndexedDeleter);
  v8::Intercepted intercepted = f(index, callback_info);
  return intercepted;
}

DirectHandle<JSObjectOrUndefined>
PropertyCallbackArguments::CallPropertyEnumerator(
    Isolate* isolate, DirectHandle<InterceptorInfo> interceptor) {
  // Named and indexed enumerator callbacks have same signatures.
  static_assert(std::is_same_v<NamedPropertyEnumeratorCallback,
                               IndexedPropertyEnumeratorCallback>);
  DCHECK(!is_setter_definer_deleter_);
  // The actual property key is not relevant for this callback.
  set_property_key(0);
  slot_at(kCallbackInfoIndex).store(*interceptor);
  // Enumerator callback's return value is initialized with undefined even
  // though it's supposed to return v8::Array.
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).undefined_value());
  // TODO(ishell): consider making it return v8::Intercepted to indicate
  // whether the result was set or not.
  IndexedPropertyEnumeratorCallback f;
  if (interceptor->is_named()) {
    f = reinterpret_cast<NamedPropertyEnumeratorCallback>(
        interceptor->named_enumerator(isolate));
  } else {
    f = reinterpret_cast<IndexedPropertyEnumeratorCallback>(
        interceptor->indexed_enumerator(isolate));
  }
  PREPARE_CALLBACK_INFO_INTERCEPTOR(isolate, f, v8::Array, interceptor,
                                    ExceptionContext::kNamedEnumerator);
  f(callback_info);
  DirectHandle<JSAny> result = GetReturnValue<JSAny>();
  DCHECK(IsUndefined(*result) || IsJSObject(*result));
  return Cast<JSObjectOrUndefined>(result);
}

// -------------------------------------------------------------------------
// Accessors

DirectHandle<JSAny> PropertyCallbackArguments::CallAccessorGetter(
    Isolate* isolate, DirectHandle<AccessorInfo> accessor_info,
    DirectHandle<Name> name) {
  DCHECK(!is_setter_definer_deleter_);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kAccessorGetterCallback);
  // Unlike interceptor callbacks we know that the property exists, so
  // the callback is allowed to have side effects.
  AcceptSideEffects();

  set_property_key(*name);
  slot_at(kCallbackInfoIndex).store(*accessor_info);
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).undefined_value());
  AccessorNameGetterCallback f = reinterpret_cast<AccessorNameGetterCallback>(
      accessor_info->getter(isolate));
  PREPARE_CALLBACK_INFO_ACCESSOR(isolate, f, v8::Value, accessor_info,
                                 receiver(), ACCESSOR_GETTER,
                                 ExceptionContext::kAttributeGet);
  f(v8::Utils::ToLocal(name), callback_info);
  return GetReturnValue<JSAny>();
}

bool PropertyCallbackArguments::CallAccessorSetter(
    Isolate* isolate, DirectHandle<AccessorInfo> accessor_info,
    DirectHandle<Name> name, DirectHandle<Object> value) {
  DCHECK(is_setter_definer_deleter_);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kAccessorSetterCallback);
  // Unlike interceptor callbacks we know that the property exists, so
  // the callback is allowed to have side effects.
  AcceptSideEffects();

  set_property_key(*name);
  slot_at(kCallbackInfoIndex).store(*accessor_info);
  slot_at(kReturnValueIndex).store(ReadOnlyRoots(isolate).true_value());
  // The actual type of setter callback is either
  // v8::AccessorNameSetterCallback or
  // i::Accessors::AccessorNameBooleanSetterCallback, depending on whether the
  // AccessorInfo was created by the API or internally (see accessors.cc).
  // Here we handle both cases using the AccessorNameSetterCallback signature
  // and checking whether the returned result is set to default value
  // (the undefined value).
  // TODO(ishell, 348660658): update V8 Api to allow setter callbacks provide
  // the result of [[Set]] operation according to JavaScript semantics.
  AccessorNameSetterCallback f = reinterpret_cast<AccessorNameSetterCallback>(
      accessor_info->setter(isolate));
  PREPARE_CALLBACK_INFO_ACCESSOR(isolate, f, void, accessor_info, receiver(),
                                 ACCESSOR_SETTER,
                                 ExceptionContext::kAttributeSet);
  f(v8::Utils::ToLocal(name), v8::Utils::ToLocal(value), callback_info);
  // Historically, in case of v8::AccessorNameSetterCallback it wasn't allowed
  // to set the result and not setting the result was treated as successful
  // execution.
  // During interceptors Api refactoring it was temporarily allowed to call
  // v8::ReturnValue<void>::Set[NonEmpty](Local<S>) and the result was just
  // converted to v8::Boolean which was then treated as a result of [[Set]].
  // In case of AccessorNameBooleanSetterCallback, the result is always
  // set to v8::Boolean or an exception is be thrown (in which case the
  // result is ignored anyway). So, regardless of whether the signature was
  // v8::AccessorNameSetterCallback or AccessorNameBooleanSetterCallback
  // the result is guaranteed to be v8::Boolean value indicating success or
  // failure.
  DirectHandle<Boolean> result = GetReturnValue<Boolean>();
  return IsTrue(*result, isolate);
}

#undef PREPARE_CALLBACK_INFO_ACCESSOR
#undef PREPARE_CALLBACK_INFO_INTERCEPTOR

}  // namespace internal
}  // namespace v8

#endif  // V8_API_API_ARGUMENTS_INL_H_
