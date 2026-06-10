#include "cppgc_helpers.h"
#include "env-inl.h"

namespace node {

void CppgcWrapperList::Cleanup() {
  for (auto node : *this) {
    CppgcMixin* ptr = node->persistent.Get();
    if (ptr != nullptr) {
      ptr->Finalize();
    }
  }
}

void CppgcWrapperList::MemoryInfo(MemoryTracker* tracker) const {
  for (auto node : *this) {
    CppgcMixin* ptr = node->persistent.Get();
    if (ptr != nullptr) {
      // TODO(addaleax): Add weak edges instead of no edges once
      // https://github.com/v8/v8/commit/e37cadf1143a8c5bbe44c0408186b5a26cc23863
      // is available for us
      tracker->Track(ptr, MemoryTracker::kWeakEdge);
    }
  }
}

void CppgcWrapperList::PurgeEmpty() {
  for (auto weak_it = begin(); weak_it != end();) {
    CppgcWrapperListNode* node = *weak_it;
    auto next_it = ++weak_it;
    // The underlying cppgc wrapper has already been garbage collected.
    // Remove it from the list.
    if (!node->persistent) {
      node->persistent.Clear();
      delete node;
    }
    weak_it = next_it;
  }
}
}  // namespace node
