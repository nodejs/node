// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_EVACUATION_ALLOCATOR_H_
#define V8_HEAP_EVACUATION_ALLOCATOR_H_

#include "src/common/globals.h"
#include "src/heap/heap.h"
#include "src/heap/new-spaces.h"
#include "src/heap/paged-spaces.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

// Allocator encapsulating thread-local allocation durning collection. Assumes
// that all other allocations also go through EvacuationAllocator.
class EvacuationAllocator {
 public:
  static const int kLabSize = 32 * KB;
  static const int kMaxLabObjectSize = 8 * KB;

  explicit EvacuationAllocator(Heap* heap,
                               CompactionSpaceKind compaction_space_kind)
      : heap_(heap),
        new_space_(heap->new_space()),
        compaction_spaces_(heap, compaction_space_kind),
        new_space_lab_(LocalAllocationBuffer::InvalidBuffer()),
        lab_allocation_will_fail_(false) {}

  // Needs to be called from the main thread to finalize this
  // EvacuationAllocator.
  void Finalize() {
    heap_->old_space()->MergeCompactionSpace(compaction_spaces_.Get(OLD_SPACE));
    heap_->code_space()->MergeCompactionSpace(
        compaction_spaces_.Get(CODE_SPACE));
    if (heap_->shared_space()) {
      heap_->shared_space()->MergeCompactionSpace(
          compaction_spaces_.Get(SHARED_SPACE));
    }

    // Give back remaining LAB space if this EvacuationAllocator's new space LAB
    // sits right next to new space allocation top.
    const LinearAllocationArea info = new_space_lab_.CloseAndMakeIterable();
    if (new_space_) new_space_->MaybeFreeUnusedLab(info);
  }

  inline AllocationResult Allocate(AllocationSpace space, int object_size,
                                   AllocationOrigin origin,
                                   AllocationAlignment alignment);
  inline void FreeLast(AllocationSpace space, HeapObject object,
                       int object_size);

 private:
  inline AllocationResult AllocateInNewSpace(int object_size,
                                             AllocationOrigin origin,
                                             AllocationAlignment alignment);
  inline bool NewLocalAllocationBuffer();
  inline AllocationResult AllocateInLAB(int object_size,
                                        AllocationAlignment alignment);
  inline void FreeLastInNewSpace(HeapObject object, int object_size);
  inline void FreeLastInCompactionSpace(AllocationSpace space,
                                        HeapObject object, int object_size);

  Heap* const heap_;
  NewSpace* const new_space_;
  CompactionSpaceCollection compaction_spaces_;
  LocalAllocationBuffer new_space_lab_;
  bool lab_allocation_will_fail_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_EVACUATION_ALLOCATOR_H_
