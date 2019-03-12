// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_API_CALLBACKS_INL_H_
#define V8_OBJECTS_API_CALLBACKS_INL_H_

#include "src/objects/api-callbacks.h"

#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/name.h"
#include "src/objects/templates.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(AccessCheckInfo, Struct)
OBJECT_CONSTRUCTORS_IMPL(AccessorInfo, Struct)
OBJECT_CONSTRUCTORS_IMPL(InterceptorInfo, Struct)
OBJECT_CONSTRUCTORS_IMPL(CallHandlerInfo, Tuple3)

CAST_ACCESSOR(AccessorInfo)
CAST_ACCESSOR(AccessCheckInfo)
CAST_ACCESSOR(InterceptorInfo)
CAST_ACCESSOR(CallHandlerInfo)

ACCESSORS(AccessorInfo, name, Name, kNameOffset)
SMI_ACCESSORS(AccessorInfo, flags, kFlagsOffset)
ACCESSORS(AccessorInfo, expected_receiver_type, Object,
          kExpectedReceiverTypeOffset)

ACCESSORS_CHECKED2(AccessorInfo, getter, Object, kGetterOffset, true,
                   Foreign::IsNormalized(value))
ACCESSORS_CHECKED2(AccessorInfo, setter, Object, kSetterOffset, true,
                   Foreign::IsNormalized(value));
ACCESSORS(AccessorInfo, js_getter, Object, kJsGetterOffset)
ACCESSORS(AccessorInfo, data, Object, kDataOffset)

bool AccessorInfo::has_getter() {
  bool result = getter() != Smi::kZero;
  DCHECK_EQ(result,
            getter() != Smi::kZero &&
                Foreign::cast(getter())->foreign_address() != kNullAddress);
  return result;
}

bool AccessorInfo::has_setter() {
  bool result = setter() != Smi::kZero;
  DCHECK_EQ(result,
            setter() != Smi::kZero &&
                Foreign::cast(setter())->foreign_address() != kNullAddress);
  return result;
}

BIT_FIELD_ACCESSORS(AccessorInfo, flags, all_can_read,
                    AccessorInfo::AllCanReadBit)
BIT_FIELD_ACCESSORS(AccessorInfo, flags, all_can_write,
                    AccessorInfo::AllCanWriteBit)
BIT_FIELD_ACCESSORS(AccessorInfo, flags, is_special_data_property,
                    AccessorInfo::IsSpecialDataPropertyBit)
BIT_FIELD_ACCESSORS(AccessorInfo, flags, replace_on_access,
                    AccessorInfo::ReplaceOnAccessBit)
BIT_FIELD_ACCESSORS(AccessorInfo, flags, is_sloppy, AccessorInfo::IsSloppyBit)
BIT_FIELD_ACCESSORS(AccessorInfo, flags, getter_side_effect_type,
                    AccessorInfo::GetterSideEffectTypeBits)

SideEffectType AccessorInfo::setter_side_effect_type() const {
  return SetterSideEffectTypeBits::decode(flags());
}

void AccessorInfo::set_setter_side_effect_type(SideEffectType value) {
  // We do not support describing setters as having no side effect, since
  // calling set accessors must go through a store bytecode. Store bytecodes
  // support checking receivers for temporary objects, but still expect
  // the receiver to be written to.
  CHECK_NE(value, SideEffectType::kHasNoSideEffect);
  set_flags(SetterSideEffectTypeBits::update(flags(), value));
}

BIT_FIELD_ACCESSORS(AccessorInfo, flags, initial_property_attributes,
                    AccessorInfo::InitialAttributesBits)

bool AccessorInfo::IsCompatibleReceiver(Object receiver) {
  if (!HasExpectedReceiverType()) return true;
  if (!receiver->IsJSObject()) return false;
  return FunctionTemplateInfo::cast(expected_receiver_type())
      ->IsTemplateFor(JSObject::cast(receiver)->map());
}

bool AccessorInfo::HasExpectedReceiverType() {
  return expected_receiver_type()->IsFunctionTemplateInfo();
}

ACCESSORS(AccessCheckInfo, callback, Object, kCallbackOffset)
ACCESSORS(AccessCheckInfo, named_interceptor, Object, kNamedInterceptorOffset)
ACCESSORS(AccessCheckInfo, indexed_interceptor, Object,
          kIndexedInterceptorOffset)
ACCESSORS(AccessCheckInfo, data, Object, kDataOffset)

ACCESSORS(InterceptorInfo, getter, Object, kGetterOffset)
ACCESSORS(InterceptorInfo, setter, Object, kSetterOffset)
ACCESSORS(InterceptorInfo, query, Object, kQueryOffset)
ACCESSORS(InterceptorInfo, descriptor, Object, kDescriptorOffset)
ACCESSORS(InterceptorInfo, deleter, Object, kDeleterOffset)
ACCESSORS(InterceptorInfo, enumerator, Object, kEnumeratorOffset)
ACCESSORS(InterceptorInfo, definer, Object, kDefinerOffset)
ACCESSORS(InterceptorInfo, data, Object, kDataOffset)
SMI_ACCESSORS(InterceptorInfo, flags, kFlagsOffset)
BOOL_ACCESSORS(InterceptorInfo, flags, can_intercept_symbols,
               kCanInterceptSymbolsBit)
BOOL_ACCESSORS(InterceptorInfo, flags, all_can_read, kAllCanReadBit)
BOOL_ACCESSORS(InterceptorInfo, flags, non_masking, kNonMasking)
BOOL_ACCESSORS(InterceptorInfo, flags, is_named, kNamed)
BOOL_ACCESSORS(InterceptorInfo, flags, has_no_side_effect, kHasNoSideEffect)

ACCESSORS(CallHandlerInfo, callback, Object, kCallbackOffset)
ACCESSORS(CallHandlerInfo, js_callback, Object, kJsCallbackOffset)
ACCESSORS(CallHandlerInfo, data, Object, kDataOffset)

bool CallHandlerInfo::IsSideEffectFreeCallHandlerInfo() const {
  ReadOnlyRoots roots = GetReadOnlyRoots();
  DCHECK(map() == roots.side_effect_call_handler_info_map() ||
         map() == roots.side_effect_free_call_handler_info_map() ||
         map() == roots.next_call_side_effect_free_call_handler_info_map());
  return map() == roots.side_effect_free_call_handler_info_map();
}

bool CallHandlerInfo::IsSideEffectCallHandlerInfo() const {
  ReadOnlyRoots roots = GetReadOnlyRoots();
  DCHECK(map() == roots.side_effect_call_handler_info_map() ||
         map() == roots.side_effect_free_call_handler_info_map() ||
         map() == roots.next_call_side_effect_free_call_handler_info_map());
  return map() == roots.side_effect_call_handler_info_map();
}

void CallHandlerInfo::SetNextCallHasNoSideEffect() {
  set_map(
      GetReadOnlyRoots().next_call_side_effect_free_call_handler_info_map());
}

bool CallHandlerInfo::NextCallHasNoSideEffect() {
  ReadOnlyRoots roots = GetReadOnlyRoots();
  if (map() == roots.next_call_side_effect_free_call_handler_info_map()) {
    set_map(roots.side_effect_call_handler_info_map());
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_API_CALLBACKS_INL_H_
