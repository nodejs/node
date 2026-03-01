// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-data-object-builder.h"

#include "src/objects/js-data-object-builder-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/lookup.h"

namespace v8 {
namespace internal {

JSDataObjectBuilder::JSDataObjectBuilder(Isolate* isolate,
                                         ElementsKind elements_kind,
                                         int expected_named_properties,
                                         DirectHandle<Map> expected_final_map,
                                         HeapNumberMode heap_number_mode)
    : isolate_(isolate),
      elements_kind_(elements_kind),
      expected_property_count_(expected_named_properties),
      heap_number_mode_(heap_number_mode),
      expected_final_map_(expected_final_map) {
  if (!TryInitializeMapFromExpectedFinalMap()) {
    InitializeMapFromZero();
  }
}

void JSDataObjectBuilder::AddSlowProperty(DirectHandle<String> key,
                                          Handle<Object> value) {
  DCHECK(!object_.is_null());

  LookupIterator it(isolate_, object_, key, object_, LookupIterator::OWN);
  JSObject::DefineOwnPropertyIgnoreAttributes(&it, value, NONE).Check();
}

bool JSDataObjectBuilder::TryInitializeMapFromExpectedFinalMap() {
  if (expected_final_map_.is_null()) return false;
  if (expected_final_map_->elements_kind() != elements_kind_) return false;

  int property_count_in_expected_final_map =
      expected_final_map_->NumberOfOwnDescriptors();
  if (property_count_in_expected_final_map < expected_property_count_)
    return false;

  map_ = expected_final_map_;
  property_count_in_expected_final_map_ = property_count_in_expected_final_map;
  return true;
}

void JSDataObjectBuilder::InitializeMapFromZero() {
  // Must be called before any properties are registered.
  DCHECK_EQ(current_property_index_, 0);

  map_ = isolate_->factory()->ObjectLiteralMapFromCache(
      isolate_->native_context(), expected_property_count_);
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
  map_ = handle(map_->FindFieldOwner(
                    isolate_, InternalIndex(current_property_index_ - 1)),
                isolate_);
}

void JSDataObjectBuilder::RewindExpectedFinalMapFastPathToIncludeCurrent() {
  DCHECK_EQ(*map_, *expected_final_map_);
  map_ = handle(expected_final_map_->FindFieldOwner(
                    isolate_, InternalIndex(current_property_index_)),
                isolate_);
}

}  // namespace internal
}  // namespace v8
