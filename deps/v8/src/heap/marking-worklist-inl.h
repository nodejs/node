// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_HEAP_MARKING_WORKLIST_INL_H_
#define V8_HEAP_MARKING_WORKLIST_INL_H_

#include <unordered_map>
#include <vector>

#include "src/heap/marking-worklist.h"

namespace v8 {
namespace internal {

template <typename Callback>
void MarkingWorklists::Update(Callback callback) {
  shared_.Update(callback);
  on_hold_.Update(callback);
  embedder_.Update(callback);
  other_.Update(callback);
  for (auto cw : context_worklists_) {
    if (cw.context == kSharedContext || cw.context == kOtherContext) {
      // These contexts were updated above.
      continue;
    }
    cw.worklist->Update(callback);
  }
}

void MarkingWorklists::Local::Push(HeapObject object) { active_.Push(object); }

bool MarkingWorklists::Local::Pop(HeapObject* object) {
  if (active_.Pop(object)) return true;
  if (!is_per_context_mode_) return false;
  // The active worklist is empty. Find any other non-empty worklist and
  // switch the active worklist to it.
  return PopContext(object);
}

void MarkingWorklists::Local::PushOnHold(HeapObject object) {
  on_hold_.Push(object);
}

bool MarkingWorklists::Local::PopOnHold(HeapObject* object) {
  return on_hold_.Pop(object);
}

void MarkingWorklists::Local::PushEmbedder(HeapObject object) {
  embedder_.Push(object);
}

bool MarkingWorklists::Local::PopEmbedder(HeapObject* object) {
  return embedder_.Pop(object);
}

Address MarkingWorklists::Local::SwitchToContext(Address context) {
  if (context == active_context_) return context;
  return SwitchToContextSlow(context);
}

Address MarkingWorklists::Local::SwitchToShared() {
  return SwitchToContext(kSharedContext);
}

void MarkingWorklists::Local::SwitchToContext(
    Address context, MarkingWorklist::Local* worklist) {
  // Save the current worklist.
  *active_owner_ = std::move(active_);
  // Switch to the new worklist.
  active_owner_ = worklist;
  active_ = std::move(*worklist);
  active_context_ = context;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MARKING_WORKLIST_INL_H_
