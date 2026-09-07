#include "cppgc_helpers.h"
#include "env-inl.h"

namespace node {

void CppgcWrapperList::Cleanup() {
  for (auto node : *this) {
    if (auto* ptr = node->persistent.Get()) {
      ptr->Finalize();
    }
  }
}

void CppgcWrapperList::MemoryInfo(MemoryTracker* tracker) const {
  for (auto node : *this) {
    if (auto* ptr = node->persistent.Get()) {
      // TODO(addaleax): Add weak edges instead of no edges once
      // https://github.com/v8/v8/commit/e37cadf1143a8c5bbe44c0408186b5a26cc23863
      // is available for us
      tracker->Track(ptr, MemoryTracker::kWeakEdge);
    }
  }
}

void CppgcWrapperList::PurgeEmpty() {
  for (auto it = begin(); it != end(); ) {
    CppgcWrapperListNode* node = *it;
    ++it; // Advance the iterator BEFORE deletion to avoid dangling references
    
    // The underlying cppgc wrapper has already been garbage collected.
    // Remove it from the list.
    if (!node->persistent) {
      delete node; 
    }
  }
}

}  // namespace node
