// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_INL_H_
#define V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_INL_H_

#include "src/heap/cppgc-js/cpp-marking-state.h"
#include "src/heap/cppgc-js/wrappable-info-inl.h"
#include "src/heap/cppgc-js/wrappable-info.h"
#include "src/objects/embedder-data-slot.h"
#include "src/objects/js-objects.h"

namespace v8 {
namespace internal {

bool CppMarkingState::ExtractEmbedderDataSnapshot(
    Tagged<Map> map, Tagged<JSObject> object, EmbedderDataSnapshot& snapshot) {
  if (JSObject::GetEmbedderFieldCount(map) < 2) return false;

  DCHECK(SupportsWrappableExtraction());

  EmbedderDataSlot::PopulateEmbedderDataSnapshot(
      map, object, wrapper_descriptor_.wrappable_type_index, snapshot.first);
  EmbedderDataSlot::PopulateEmbedderDataSnapshot(
      map, object, wrapper_descriptor_.wrappable_instance_index,
      snapshot.second);
  return true;
}

void CppMarkingState::MarkAndPush(const EmbedderDataSnapshot& snapshot) {
  const EmbedderDataSlot type_slot(snapshot.first);
  const EmbedderDataSlot instance_slot(snapshot.second);
  MarkAndPush(type_slot, instance_slot);
}

void CppMarkingState::MarkAndPush(const EmbedderDataSlot type_slot,
                                  const EmbedderDataSlot instance_slot) {
  DCHECK(SupportsWrappableExtraction());
  const auto maybe_info = WrappableInfo::From(
      isolate_, type_slot, instance_slot, wrapper_descriptor_);
  if (maybe_info.has_value()) {
    marking_state_.MarkAndPush(
        cppgc::internal::HeapObjectHeader::FromObject(maybe_info->instance));
  }
}

void CppMarkingState::MarkAndPush(void* instance) {
  marking_state_.MarkAndPush(
      cppgc::internal::HeapObjectHeader::FromObject(instance));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_INL_H_
