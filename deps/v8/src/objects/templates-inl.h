// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATES_INL_H_
#define V8_OBJECTS_TEMPLATES_INL_H_

#include "src/objects/templates.h"

#include "src/heap/heap-inl.h"
#include "src/objects/shared-function-info-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

ACCESSORS(TemplateInfo, tag, Object, kTagOffset)
ACCESSORS(TemplateInfo, serial_number, Object, kSerialNumberOffset)
SMI_ACCESSORS(TemplateInfo, number_of_properties, kNumberOfProperties)
ACCESSORS(TemplateInfo, property_list, Object, kPropertyListOffset)
ACCESSORS(TemplateInfo, property_accessors, Object, kPropertyAccessorsOffset)

ACCESSORS(FunctionTemplateInfo, call_code, Object, kCallCodeOffset)
ACCESSORS(FunctionTemplateInfo, prototype_template, Object,
          kPrototypeTemplateOffset)
ACCESSORS(FunctionTemplateInfo, prototype_provider_template, Object,
          kPrototypeProviderTemplateOffset)
ACCESSORS(FunctionTemplateInfo, parent_template, Object, kParentTemplateOffset)
ACCESSORS(FunctionTemplateInfo, named_property_handler, Object,
          kNamedPropertyHandlerOffset)
ACCESSORS(FunctionTemplateInfo, indexed_property_handler, Object,
          kIndexedPropertyHandlerOffset)
ACCESSORS(FunctionTemplateInfo, instance_template, Object,
          kInstanceTemplateOffset)
ACCESSORS(FunctionTemplateInfo, class_name, Object, kClassNameOffset)
ACCESSORS(FunctionTemplateInfo, signature, Object, kSignatureOffset)
ACCESSORS(FunctionTemplateInfo, instance_call_handler, Object,
          kInstanceCallHandlerOffset)
ACCESSORS(FunctionTemplateInfo, access_check_info, Object,
          kAccessCheckInfoOffset)
ACCESSORS(FunctionTemplateInfo, shared_function_info, Object,
          kSharedFunctionInfoOffset)
ACCESSORS(FunctionTemplateInfo, cached_property_name, Object,
          kCachedPropertyNameOffset)
SMI_ACCESSORS(FunctionTemplateInfo, length, kLengthOffset)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, hidden_prototype,
               kHiddenPrototypeBit)
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

ACCESSORS(ObjectTemplateInfo, constructor, Object, kConstructorOffset)
ACCESSORS(ObjectTemplateInfo, data, Object, kDataOffset)

CAST_ACCESSOR(TemplateInfo)
CAST_ACCESSOR(FunctionTemplateInfo)
CAST_ACCESSOR(ObjectTemplateInfo)

bool FunctionTemplateInfo::instantiated() {
  return shared_function_info()->IsSharedFunctionInfo();
}

bool FunctionTemplateInfo::BreakAtEntry() {
  Object* maybe_shared = shared_function_info();
  if (maybe_shared->IsSharedFunctionInfo()) {
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(maybe_shared);
    return shared->BreakAtEntry();
  }
  return false;
}

FunctionTemplateInfo* FunctionTemplateInfo::GetParent(Isolate* isolate) {
  Object* parent = parent_template();
  return parent->IsUndefined(isolate) ? nullptr
                                      : FunctionTemplateInfo::cast(parent);
}

ObjectTemplateInfo* ObjectTemplateInfo::GetParent(Isolate* isolate) {
  Object* maybe_ctor = constructor();
  if (maybe_ctor->IsUndefined(isolate)) return nullptr;
  FunctionTemplateInfo* constructor = FunctionTemplateInfo::cast(maybe_ctor);
  while (true) {
    constructor = constructor->GetParent(isolate);
    if (constructor == nullptr) return nullptr;
    Object* maybe_obj = constructor->instance_template();
    if (!maybe_obj->IsUndefined(isolate)) {
      return ObjectTemplateInfo::cast(maybe_obj);
    }
  }
  return nullptr;
}

int ObjectTemplateInfo::embedder_field_count() const {
  Object* value = data();
  DCHECK(value->IsSmi());
  return EmbedderFieldCount::decode(Smi::ToInt(value));
}

void ObjectTemplateInfo::set_embedder_field_count(int count) {
  DCHECK_LE(count, JSObject::kMaxEmbedderFields);
  return set_data(
      Smi::FromInt(EmbedderFieldCount::update(Smi::ToInt(data()), count)));
}

bool ObjectTemplateInfo::immutable_proto() const {
  Object* value = data();
  DCHECK(value->IsSmi());
  return IsImmutablePrototype::decode(Smi::ToInt(value));
}

void ObjectTemplateInfo::set_immutable_proto(bool immutable) {
  return set_data(Smi::FromInt(
      IsImmutablePrototype::update(Smi::ToInt(data()), immutable)));
}

bool FunctionTemplateInfo::IsTemplateFor(JSObject* object) {
  return IsTemplateFor(object->map());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATES_INL_H_
