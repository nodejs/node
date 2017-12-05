// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAP_INL_H_
#define V8_OBJECTS_MAP_INL_H_

#include "src/field-type.h"
#include "src/objects/map.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(Map)

InterceptorInfo* Map::GetNamedInterceptor() {
  DCHECK(has_named_interceptor());
  FunctionTemplateInfo* info = GetFunctionTemplateInfo();
  return InterceptorInfo::cast(info->named_property_handler());
}

InterceptorInfo* Map::GetIndexedInterceptor() {
  DCHECK(has_indexed_interceptor());
  FunctionTemplateInfo* info = GetFunctionTemplateInfo();
  return InterceptorInfo::cast(info->indexed_property_handler());
}

bool Map::IsInplaceGeneralizableField(PropertyConstness constness,
                                      Representation representation,
                                      FieldType* field_type) {
  if (FLAG_track_constant_fields && FLAG_modify_map_inplace &&
      (constness == kConst)) {
    // kConst -> kMutable field generalization may happen in-place.
    return true;
  }
  if (representation.IsHeapObject() && !field_type->IsAny()) {
    return true;
  }
  return false;
}

bool Map::CanHaveFastTransitionableElementsKind(InstanceType instance_type) {
  return instance_type == JS_ARRAY_TYPE || instance_type == JS_VALUE_TYPE ||
         instance_type == JS_ARGUMENTS_TYPE;
}

bool Map::CanHaveFastTransitionableElementsKind() const {
  return CanHaveFastTransitionableElementsKind(instance_type());
}

// static
void Map::GeneralizeIfCanHaveTransitionableFastElementsKind(
    Isolate* isolate, InstanceType instance_type, PropertyConstness* constness,
    Representation* representation, Handle<FieldType>* field_type) {
  if (CanHaveFastTransitionableElementsKind(instance_type)) {
    // We don't support propagation of field generalization through elements
    // kind transitions because they are inserted into the transition tree
    // before field transitions. In order to avoid complexity of handling
    // such a case we ensure that all maps with transitionable elements kinds
    // do not have fields that can be generalized in-place (without creation
    // of a new map).
    if (FLAG_track_constant_fields && FLAG_modify_map_inplace) {
      // The constness is either already kMutable or should become kMutable if
      // it was kConst.
      *constness = kMutable;
    }
    if (representation->IsHeapObject()) {
      // The field type is either already Any or should become Any if it was
      // something else.
      *field_type = FieldType::Any(isolate);
    }
  }
}

int NormalizedMapCache::GetIndex(Handle<Map> map) {
  return map->Hash() % NormalizedMapCache::kEntries;
}

bool NormalizedMapCache::IsNormalizedMapCache(const HeapObject* obj) {
  if (!obj->IsFixedArray()) return false;
  if (FixedArray::cast(obj)->length() != NormalizedMapCache::kEntries) {
    return false;
  }
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    reinterpret_cast<NormalizedMapCache*>(const_cast<HeapObject*>(obj))
        ->NormalizedMapCacheVerify();
  }
#endif
  return true;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MAP_INL_H_
