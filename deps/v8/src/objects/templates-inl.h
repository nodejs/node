// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATES_INL_H_
#define V8_OBJECTS_TEMPLATES_INL_H_

#include "src/objects/templates.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/shared-function-info-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(TemplateInfo, Struct)
OBJECT_CONSTRUCTORS_IMPL(FunctionTemplateInfo, TemplateInfo)
OBJECT_CONSTRUCTORS_IMPL(ObjectTemplateInfo, TemplateInfo)
OBJECT_CONSTRUCTORS_IMPL(FunctionTemplateRareData, Struct)

NEVER_READ_ONLY_SPACE_IMPL(TemplateInfo)

ACCESSORS(TemplateInfo, tag, Object, kTagOffset)
ACCESSORS(TemplateInfo, serial_number, Object, kSerialNumberOffset)
SMI_ACCESSORS(TemplateInfo, number_of_properties, kNumberOfPropertiesOffset)
ACCESSORS(TemplateInfo, property_list, Object, kPropertyListOffset)
ACCESSORS(TemplateInfo, property_accessors, Object, kPropertyAccessorsOffset)

ACCESSORS(FunctionTemplateInfo, call_code, Object, kCallCodeOffset)
ACCESSORS(FunctionTemplateInfo, class_name, Object, kClassNameOffset)
ACCESSORS(FunctionTemplateInfo, signature, Object, kSignatureOffset)
ACCESSORS(FunctionTemplateInfo, shared_function_info, Object,
          kSharedFunctionInfoOffset)
ACCESSORS(FunctionTemplateInfo, rare_data, HeapObject,
          kFunctionTemplateRareDataOffset)
ACCESSORS(FunctionTemplateInfo, cached_property_name, Object,
          kCachedPropertyNameOffset)
SMI_ACCESSORS(FunctionTemplateInfo, length, kLengthOffset)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, undetectable, kUndetectableBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, needs_access_check,
               kNeedsAccessCheckBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, read_only_prototype,
               kReadOnlyPrototypeBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, remove_prototype,
               kRemovePrototypeBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, do_not_cache, kDoNotCacheBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, accept_any_receiver,
               kAcceptAnyReceiver)
SMI_ACCESSORS(FunctionTemplateInfo, flag, kFlagOffset)

// static
FunctionTemplateRareData FunctionTemplateInfo::EnsureFunctionTemplateRareData(
    Isolate* isolate, Handle<FunctionTemplateInfo> function_template_info) {
  HeapObject extra = function_template_info->rare_data(isolate);
  if (extra.IsUndefined(isolate)) {
    return AllocateFunctionTemplateRareData(isolate, function_template_info);
  } else {
    return FunctionTemplateRareData::cast(extra);
  }
}

#define RARE_ACCESSORS(Name, CamelName, Type)                                 \
  DEF_GETTER(FunctionTemplateInfo, Get##CamelName, Type) {                    \
    HeapObject extra = rare_data(isolate);                                    \
    HeapObject undefined = GetReadOnlyRoots(isolate).undefined_value();       \
    return extra == undefined ? undefined                                     \
                              : FunctionTemplateRareData::cast(extra).Name(); \
  }                                                                           \
  inline void FunctionTemplateInfo::Set##CamelName(                           \
      Isolate* isolate, Handle<FunctionTemplateInfo> function_template_info,  \
      Handle<Type> Name) {                                                    \
    FunctionTemplateRareData rare_data =                                      \
        EnsureFunctionTemplateRareData(isolate, function_template_info);      \
    rare_data.set_##Name(*Name);                                              \
  }

RARE_ACCESSORS(prototype_template, PrototypeTemplate, Object)
RARE_ACCESSORS(prototype_provider_template, PrototypeProviderTemplate, Object)
RARE_ACCESSORS(parent_template, ParentTemplate, Object)
RARE_ACCESSORS(named_property_handler, NamedPropertyHandler, Object)
RARE_ACCESSORS(indexed_property_handler, IndexedPropertyHandler, Object)
RARE_ACCESSORS(instance_template, InstanceTemplate, Object)
RARE_ACCESSORS(instance_call_handler, InstanceCallHandler, Object)
RARE_ACCESSORS(access_check_info, AccessCheckInfo, Object)
#undef RARE_ACCESSORS

ACCESSORS(FunctionTemplateRareData, prototype_template, Object,
          kPrototypeTemplateOffset)
ACCESSORS(FunctionTemplateRareData, prototype_provider_template, Object,
          kPrototypeProviderTemplateOffset)
ACCESSORS(FunctionTemplateRareData, parent_template, Object,
          kParentTemplateOffset)
ACCESSORS(FunctionTemplateRareData, named_property_handler, Object,
          kNamedPropertyHandlerOffset)
ACCESSORS(FunctionTemplateRareData, indexed_property_handler, Object,
          kIndexedPropertyHandlerOffset)
ACCESSORS(FunctionTemplateRareData, instance_template, Object,
          kInstanceTemplateOffset)
ACCESSORS(FunctionTemplateRareData, instance_call_handler, Object,
          kInstanceCallHandlerOffset)
ACCESSORS(FunctionTemplateRareData, access_check_info, Object,
          kAccessCheckInfoOffset)

ACCESSORS(ObjectTemplateInfo, constructor, Object, kConstructorOffset)
ACCESSORS(ObjectTemplateInfo, data, Object, kDataOffset)

CAST_ACCESSOR(TemplateInfo)
CAST_ACCESSOR(FunctionTemplateInfo)
CAST_ACCESSOR(FunctionTemplateRareData)
CAST_ACCESSOR(ObjectTemplateInfo)

bool FunctionTemplateInfo::instantiated() {
  return shared_function_info().IsSharedFunctionInfo();
}

bool FunctionTemplateInfo::BreakAtEntry() {
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
  Object value = data();
  DCHECK(value.IsSmi());
  return EmbedderFieldCount::decode(Smi::ToInt(value));
}

void ObjectTemplateInfo::set_embedder_field_count(int count) {
  DCHECK_LE(count, JSObject::kMaxEmbedderFields);
  return set_data(
      Smi::FromInt(EmbedderFieldCount::update(Smi::ToInt(data()), count)));
}

bool ObjectTemplateInfo::immutable_proto() const {
  Object value = data();
  DCHECK(value.IsSmi());
  return IsImmutablePrototype::decode(Smi::ToInt(value));
}

void ObjectTemplateInfo::set_immutable_proto(bool immutable) {
  return set_data(Smi::FromInt(
      IsImmutablePrototype::update(Smi::ToInt(data()), immutable)));
}

bool FunctionTemplateInfo::IsTemplateFor(JSObject object) {
  return IsTemplateFor(object.map());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATES_INL_H_
