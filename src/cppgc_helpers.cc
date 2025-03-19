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
      tracker->Track(ptr);
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
