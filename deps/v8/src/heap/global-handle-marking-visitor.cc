// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/global-handle-marking-visitor.h"

#include "src/heap/marking-worklist-inl.h"

namespace v8 {
namespace internal {

GlobalHandleMarkingVisitor::GlobalHandleMarkingVisitor(
    Heap& heap, MarkingState& marking_state,
    MarkingWorklists::Local& local_marking_worklist)
    : heap_(heap),
      marking_state_(marking_state),
      local_marking_worklist_(local_marking_worklist),
      traced_node_bounds_(
          heap.isolate()->global_handles()->GetTracedNodeBounds()) {}

void GlobalHandleMarkingVisitor::VisitPointer(const void* address) {
  const auto upper_it = std::upper_bound(
      traced_node_bounds_.begin(), traced_node_bounds_.end(), address,
      [](const void* needle, const auto& pair) { return needle < pair.first; });
  // Also checks emptiness as begin() == end() on empty bounds.
  if (upper_it == traced_node_bounds_.begin()) return;

  const auto bounds = std::next(upper_it, -1);
  if (address < bounds->second) {
    auto object = GlobalHandles::MarkTracedConservatively(
        const_cast<Address*>(reinterpret_cast<const Address*>(address)),
        const_cast<Address*>(reinterpret_cast<const Address*>(bounds->first)));
    if (!object.IsHeapObject()) {
      // The embedder is not aware of whether numbers are materialized as heap
      // objects are just passed around as Smis. This branch also filters out
      // intentionally passed `Smi::zero()` that indicate that there's no
      // object to mark.
      return;
    }
    HeapObject heap_object = HeapObject::cast(object);
    if (marking_state_.WhiteToGrey(heap_object)) {
      local_marking_worklist_.Push(heap_object);
    }
    if (V8_UNLIKELY(v8_flags.track_retaining_path)) {
      heap_.AddRetainingRoot(Root::kWrapperTracing, heap_object);
    }
  }
}

}  // namespace internal
}  // namespace v8
