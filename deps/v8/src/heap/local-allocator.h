// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/globals.h"
#include "src/heap/heap.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

// Allocator encapsulating thread-local allocation. Assumes that all other
// allocations also go through LocalAllocator.
class LocalAllocator {
 public:
  static const int kLabSize = 32 * KB;
  static const int kMaxLabObjectSize = 8 * KB;

  explicit LocalAllocator(Heap* heap)
      : heap_(heap),
        new_space_(heap->new_space()),
        compaction_spaces_(heap),
        new_space_lab_(LocalAllocationBuffer::InvalidBuffer()) {}

  // Needs to be called from the main thread to finalize this LocalAllocator.
  void Finalize() {
    heap_->old_space()->MergeCompactionSpace(compaction_spaces_.Get(OLD_SPACE));
    // Give back remaining LAB space if this LocalAllocator's new space LAB
    // sits right next to new space allocation top.
    const AllocationInfo info = new_space_lab_.Close();
    const Address top = new_space_->top();
    if (info.limit() != nullptr && info.limit() == top) {
      DCHECK_NOT_NULL(info.top());
      *new_space_->allocation_top_address() = info.top();
    }
  }

  template <AllocationSpace space>
  AllocationResult Allocate(int object_size, AllocationAlignment alignment) {
    switch (space) {
      case NEW_SPACE:
        return AllocateInNewSpace(object_size, alignment);
      case OLD_SPACE:
        return compaction_spaces_.Get(OLD_SPACE)->AllocateRaw(object_size,
                                                              alignment);
      default:
        // Only new and old space supported.
        UNREACHABLE();
        break;
    }
  }

 private:
  AllocationResult AllocateInNewSpace(int object_size,
                                      AllocationAlignment alignment) {
    if (object_size > kMaxLabObjectSize) {
      return new_space_->AllocateRawSynchronized(object_size, alignment);
    }
    return AllocateInLAB(object_size, alignment);
  }

  inline bool NewLocalAllocationBuffer() {
    LocalAllocationBuffer saved_lab_ = new_space_lab_;
    AllocationResult result =
        new_space_->AllocateRawSynchronized(kLabSize, kWordAligned);
    new_space_lab_ = LocalAllocationBuffer::FromResult(heap_, result, kLabSize);
    if (new_space_lab_.IsValid()) {
      new_space_lab_.TryMerge(&saved_lab_);
      return true;
    }
    return false;
  }

  AllocationResult AllocateInLAB(int object_size,
                                 AllocationAlignment alignment) {
    AllocationResult allocation;
    if (!new_space_lab_.IsValid() && !NewLocalAllocationBuffer()) {
      return AllocationResult::Retry(OLD_SPACE);
    }
    allocation = new_space_lab_.AllocateRawAligned(object_size, alignment);
    if (allocation.IsRetry()) {
      if (!NewLocalAllocationBuffer()) {
        return AllocationResult::Retry(OLD_SPACE);
      } else {
        allocation = new_space_lab_.AllocateRawAligned(object_size, alignment);
        CHECK(!allocation.IsRetry());
      }
    }
    return allocation;
  }

  Heap* const heap_;
  NewSpace* const new_space_;
  CompactionSpaceCollection compaction_spaces_;
  LocalAllocationBuffer new_space_lab_;
};

}  // namespace internal
}  // namespace v8
