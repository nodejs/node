// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_INL_H_
#define V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_INL_H_

#include "src/heap/cppgc-js/cpp-marking-state.h"
#include "src/heap/cppgc-js/wrappable-info-inl.h"
#include "src/heap/cppgc-js/wrappable-info.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/objects/embedder-data-slot.h"
#include "src/objects/js-objects.h"

namespace v8 {
namespace internal {

bool CppMarkingState::ExtractEmbedderDataSnapshot(
    Map map, JSObject object, EmbedderDataSnapshot& snapshot) {
  if (JSObject::GetEmbedderFieldCount(map) < 2) return false;

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
  const auto maybe_info = WrappableInfo::From(
      isolate_, type_slot, instance_slot, wrapper_descriptor_);
  if (maybe_info.has_value()) {
    marking_state_.MarkAndPush(
        cppgc::internal::HeapObjectHeader::FromObject(maybe_info->instance));
  }
}

// TODO(v8:13796): Remove this if it doesn't flush out any issues.
void CppMarkingState::MarkAndPushForWriteBarrier(
    const EmbedderDataSlot type_slot, const EmbedderDataSlot instance_slot) {
  const auto maybe_info = WrappableInfo::From(
      isolate_, type_slot, instance_slot, wrapper_descriptor_);
  if (maybe_info.has_value()) {
    cppgc::internal::HeapObjectHeader& header =
        cppgc::internal::HeapObjectHeader::FromObject(maybe_info->instance);
    CHECK_EQ(&header, &cppgc::internal::BasePage::FromPayload(&header)
                           ->ObjectHeaderFromInnerAddress(&header));
    marking_state_.MarkAndPush(header);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_CPP_MARKING_STATE_INL_H_
