// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/evacuation-allocator.h"

#include "src/heap/main-allocator-inl.h"

namespace v8 {
namespace internal {

void EvacuationAllocator::Finalize() {
  heap_->old_space()->MergeCompactionSpace(compaction_spaces_.Get(OLD_SPACE));
  heap_->code_space()->MergeCompactionSpace(compaction_spaces_.Get(CODE_SPACE));
  if (heap_->shared_space()) {
    heap_->shared_space()->MergeCompactionSpace(
        compaction_spaces_.Get(SHARED_SPACE));
  }
  heap_->trusted_space()->MergeCompactionSpace(
      compaction_spaces_.Get(TRUSTED_SPACE));

  // Give back remaining LAB space if this EvacuationAllocator's new space LAB
  // sits right next to new space allocation top.
  const LinearAllocationArea info = new_space_lab_.CloseAndMakeIterable();
  if (new_space_) new_space_->main_allocator()->MaybeFreeUnusedLab(info);
}

}  // namespace internal
}  // namespace v8
