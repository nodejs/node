// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-data-object-builder.h"

#include "src/heap/factory.h"
#include "src/objects/js-data-object-builder-inl.h"
#include "src/objects/lookup.h"

namespace v8 {
namespace internal {

JSDataObjectBuilder::JSDataObjectBuilder(Isolate* isolate,
                                         ElementsKind elements_kind,
                                         int expected_named_properties,
                                         DirectHandle<Map> expected_final_map,
                                         HeapNumberMode heap_number_mode,
                                         DirectHandle<JSPrototype> prototype)
    : isolate_(isolate),
      elements_kind_(elements_kind),
      expected_property_count_(expected_named_properties),
      heap_number_mode_(heap_number_mode),
      may_have_duplicate_keys_(true),
      expected_final_map_(expected_final_map),
      prototype_(prototype) {
  if (!TryInitializeMapFromExpectedFinalMap()) {
    InitializeMapFromZero();
  }
}

void JSDataObjectBuilder::AddSlowProperty(DirectHandle<InternalizedString> key,
                                          Handle<Object> value) {
  DCHECK(!object_.is_null());

  LookupIterator it(isolate_, object_, key, object_, LookupIterator::OWN);
  JSObject::DefineOwnPropertyIgnoreAttributes(&it, value, NONE).Check();
}

bool JSDataObjectBuilder::TryInitializeMapFromExpectedFinalMap() {
  if (expected_final_map_.is_null()) return false;
  // Callers that request a specific prototype don't pass an expected final
  // map, so the prototype of the expected final map is never overridden.
  DCHECK(prototype_.is_null());
  if (expected_final_map_->elements_kind() != elements_kind_) return false;

  int property_count_in_expected_final_map =
      expected_final_map_->NumberOfOwnDescriptors();
  if (property_count_in_expected_final_map < expected_property_count_) {
    return false;
  }

  map_ = expected_final_map_;
  property_count_in_expected_final_map_ = property_count_in_expected_final_map;
  return true;
}

void JSDataObjectBuilder::InitializeMapFromZero() {
  // Must be called before any properties are registered.
  DCHECK_EQ(current_property_index_, 0);

  map_ = isolate_->factory()->ObjectLiteralMapFromCache(
      isolate_->native_context(), expected_property_count_);
  if (!prototype_.is_null() && map_->prototype() != *prototype_) {
    // Transition the root map to the requested prototype before any other
    // transition, so that the property transitions below are added to the
    // prototype-transitioned root and the object stays in fast mode. Callers
    // must not request a prototype for literals that are too large for the
    // map cache: prototype transitions on the resulting dictionary map are
    // not cached, so every call would allocate a new map.
    DCHECK(!map_->is_dictionary_map());
    map_ = Map::TransitionRootMapToPrototypeForNewObject(isolate_, map_,
                                                         prototype_);
  }
  if (elements_kind_ == DICTIONARY_ELEMENTS) {
    map_ = Map::AsElementsKind(isolate_, map_, elements_kind_);
  } else {
    DCHECK_EQ(map_->elements_kind(), elements_kind_);
  }
}

void JSDataObjectBuilder::RewindExpectedFinalMapFastPathToBeforeCurrent() {
  DCHECK_GT(property_count_in_expected_final_map_, 0);
  if (current_property_index_ == 0) {
    InitializeMapFromZero();
    DCHECK_EQ(0, map_->NumberOfOwnDescriptors());
  }
  if (current_property_index_ == 0) {
    return;
  }
  DCHECK_EQ(*map_, *expected_final_map_);
  map_ =
      handle(map_->FindFieldOwner(InternalIndex(current_property_index_ - 1)),
             isolate_);
}

void JSDataObjectBuilder::RewindExpectedFinalMapFastPathToIncludeCurrent() {
  DCHECK_EQ(*map_, *expected_final_map_);
  map_ = handle(expected_final_map_->FindFieldOwner(
                    InternalIndex(current_property_index_)),
                isolate_);
}

}  // namespace internal
}  // namespace v8
