#include "cppgc_helpers.h"  // NOLINT(build/include_inline)
#include "cppgc_helpers-inl.h"

namespace node {

void CppgcWrapperList::Cleanup() {
  while (!IsEmpty()) {
    CppgcWrapperListNode* node = PopFront();
    CppgcMixin* wrapper = node->persistent.Get();
    if (wrapper != nullptr) wrapper->Finalize();
    node->realm = nullptr;
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
}  // namespace node
