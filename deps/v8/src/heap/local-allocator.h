// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LOCAL_ALLOCATOR_H_
#define V8_HEAP_LOCAL_ALLOCATOR_H_

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
        new_space_lab_(LocalAllocationBuffer::InvalidBuffer()),
        lab_allocation_will_fail_(false) {}

  // Needs to be called from the main thread to finalize this LocalAllocator.
  void Finalize() {
    heap_->old_space()->MergeCompactionSpace(compaction_spaces_.Get(OLD_SPACE));
    heap_->code_space()->MergeCompactionSpace(
        compaction_spaces_.Get(CODE_SPACE));
    // Give back remaining LAB space if this LocalAllocator's new space LAB
    // sits right next to new space allocation top.
    const LinearAllocationArea info = new_space_lab_.Close();
    const Address top = new_space_->top();
    if (info.limit() != kNullAddress && info.limit() == top) {
      DCHECK_NE(info.top(), kNullAddress);
      *new_space_->allocation_top_address() = info.top();
    }
  }

  inline AllocationResult Allocate(AllocationSpace space, int object_size,
                                   AllocationAlignment alignment);
  inline void FreeLast(AllocationSpace space, HeapObject object,
                       int object_size);

 private:
  inline AllocationResult AllocateInNewSpace(int object_size,
                                             AllocationAlignment alignment);
  inline bool NewLocalAllocationBuffer();
  inline AllocationResult AllocateInLAB(int object_size,
                                        AllocationAlignment alignment);
  inline void FreeLastInNewSpace(HeapObject object, int object_size);
  inline void FreeLastInOldSpace(HeapObject object, int object_size);

  Heap* const heap_;
  NewSpace* const new_space_;
  CompactionSpaceCollection compaction_spaces_;
  LocalAllocationBuffer new_space_lab_;
  bool lab_allocation_will_fail_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_LOCAL_ALLOCATOR_H_
