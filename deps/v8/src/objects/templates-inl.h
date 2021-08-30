// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATES_INL_H_
#define V8_OBJECTS_TEMPLATES_INL_H_

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/templates.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/templates-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(TemplateInfo)
TQ_OBJECT_CONSTRUCTORS_IMPL(FunctionTemplateInfo)
TQ_OBJECT_CONSTRUCTORS_IMPL(ObjectTemplateInfo)
TQ_OBJECT_CONSTRUCTORS_IMPL(FunctionTemplateRareData)

NEVER_READ_ONLY_SPACE_IMPL(TemplateInfo)

BOOL_ACCESSORS(FunctionTemplateInfo, flag, undetectable,
               UndetectableBit::kShift)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, needs_access_check,
               NeedsAccessCheckBit::kShift)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, read_only_prototype,
               ReadOnlyPrototypeBit::kShift)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, remove_prototype,
               RemovePrototypeBit::kShift)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, accept_any_receiver,
               AcceptAnyReceiverBit::kShift)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, published, PublishedBit::kShift)

BIT_FIELD_ACCESSORS(FunctionTemplateInfo, flag, allowed_receiver_range_start,
                    FunctionTemplateInfo::AllowedReceiverRangeStartBits)
BIT_FIELD_ACCESSORS(FunctionTemplateInfo, flag, allowed_receiver_range_end,
                    FunctionTemplateInfo::AllowedReceiverRangeEndBits)

// static
FunctionTemplateRareData FunctionTemplateInfo::EnsureFunctionTemplateRareData(
    Isolate* isolate, Handle<FunctionTemplateInfo> function_template_info) {
  HeapObject extra = function_template_info->rare_data(isolate, kAcquireLoad);
  if (extra.IsUndefined(isolate)) {
    return AllocateFunctionTemplateRareData(isolate, function_template_info);
  } else {
    return FunctionTemplateRareData::cast(extra);
  }
}

#define RARE_ACCESSORS(Name, CamelName, Type, Default)                        \
  DEF_GETTER(FunctionTemplateInfo, Get##CamelName, Type) {                    \
    HeapObject extra = rare_data(cage_base, kAcquireLoad);                    \
    HeapObject undefined = GetReadOnlyRoots(cage_base).undefined_value();     \
    return extra == undefined ? Default                                       \
                              : FunctionTemplateRareData::cast(extra).Name(); \
  }                                                                           \
  inline void FunctionTemplateInfo::Set##CamelName(                           \
      Isolate* isolate, Handle<FunctionTemplateInfo> function_template_info,  \
      Handle<Type> Name) {                                                    \
    FunctionTemplateRareData rare_data =                                      \
        EnsureFunctionTemplateRareData(isolate, function_template_info);      \
    rare_data.set_##Name(*Name);                                              \
  }

RARE_ACCESSORS(prototype_template, PrototypeTemplate, HeapObject, undefined)
RARE_ACCESSORS(prototype_provider_template, PrototypeProviderTemplate,
               HeapObject, undefined)
RARE_ACCESSORS(parent_template, ParentTemplate, HeapObject, undefined)
RARE_ACCESSORS(named_property_handler, NamedPropertyHandler, HeapObject,
               undefined)
RARE_ACCESSORS(indexed_property_handler, IndexedPropertyHandler, HeapObject,
               undefined)
RARE_ACCESSORS(instance_template, InstanceTemplate, HeapObject, undefined)
RARE_ACCESSORS(instance_call_handler, InstanceCallHandler, HeapObject,
               undefined)
RARE_ACCESSORS(access_check_info, AccessCheckInfo, HeapObject, undefined)
RARE_ACCESSORS(c_function_overloads, CFunctionOverloads, FixedArray,
               GetReadOnlyRoots(cage_base).empty_fixed_array())
#undef RARE_ACCESSORS

int FunctionTemplateInfo::InstanceType() const { return instance_type(); }

void FunctionTemplateInfo::SetInstanceType(int instance_type) {
  set_instance_type(instance_type);
}

bool TemplateInfo::should_cache() const {
  return serial_number() != kDoNotCache;
}
bool TemplateInfo::is_cached() const { return serial_number() > kUncached; }

bool FunctionTemplateInfo::instantiated() {
  return shared_function_info().IsSharedFunctionInfo();
}

inline bool FunctionTemplateInfo::BreakAtEntry() {
  Object maybe_shared = shared_function_info();
  if (maybe_shared.IsSharedFunctionInfo()) {
    SharedFunctionInfo shared = SharedFunctionInfo::cast(maybe_shared);
    return shared.BreakAtEntry();
  }
  return false;
}

FunctionTemplateInfo FunctionTemplateInfo::GetParent(Isolate* isolate) {
  Object parent = GetParentTemplate();
  return parent.IsUndefined(isolate) ? FunctionTemplateInfo()
                                     : FunctionTemplateInfo::cast(parent);
}

ObjectTemplateInfo ObjectTemplateInfo::GetParent(Isolate* isolate) {
  Object maybe_ctor = constructor();
  if (maybe_ctor.IsUndefined(isolate)) return ObjectTemplateInfo();
  FunctionTemplateInfo constructor = FunctionTemplateInfo::cast(maybe_ctor);
  while (true) {
    constructor = constructor.GetParent(isolate);
    if (constructor.is_null()) return ObjectTemplateInfo();
    Object maybe_obj = constructor.GetInstanceTemplate();
    if (!maybe_obj.IsUndefined(isolate)) {
      return ObjectTemplateInfo::cast(maybe_obj);
    }
  }
  return ObjectTemplateInfo();
}

int ObjectTemplateInfo::embedder_field_count() const {
  return EmbedderFieldCountBits::decode(data());
}

void ObjectTemplateInfo::set_embedder_field_count(int count) {
  DCHECK_LE(count, JSObject::kMaxEmbedderFields);
  return set_data(EmbedderFieldCountBits::update(data(), count));
}

bool ObjectTemplateInfo::immutable_proto() const {
  return IsImmutablePrototypeBit::decode(data());
}

void ObjectTemplateInfo::set_immutable_proto(bool immutable) {
  return set_data(IsImmutablePrototypeBit::update(data(), immutable));
}

bool ObjectTemplateInfo::code_like() const {
  return IsCodeKindBit::decode(data());
}

void ObjectTemplateInfo::set_code_like(bool is_code_like) {
  return set_data(IsCodeKindBit::update(data(), is_code_like));
}

bool FunctionTemplateInfo::IsTemplateFor(JSObject object) {
  return IsTemplateFor(object.map());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATES_INL_H_
