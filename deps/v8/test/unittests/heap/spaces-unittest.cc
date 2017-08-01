// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/isolate.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

typedef TestWithIsolate SpacesTest;

TEST_F(SpacesTest, CompactionSpaceMerge) {
  Heap* heap = i_isolate()->heap();
  OldSpace* old_space = heap->old_space();
  EXPECT_TRUE(old_space != NULL);

  CompactionSpace* compaction_space =
      new CompactionSpace(heap, OLD_SPACE, NOT_EXECUTABLE);
  EXPECT_TRUE(compaction_space != NULL);
  EXPECT_TRUE(compaction_space->SetUp());

  for (Page* p : *old_space) {
    // Unlink free lists from the main space to avoid reusing the memory for
    // compaction spaces.
    old_space->UnlinkFreeListCategories(p);
  }

  // Cannot loop until "Available()" since we initially have 0 bytes available
  // and would thus neither grow, nor be able to allocate an object.
  const int kNumObjects = 10;
  const int kNumObjectsPerPage =
      compaction_space->AreaSize() / kMaxRegularHeapObjectSize;
  const int kExpectedPages =
      (kNumObjects + kNumObjectsPerPage - 1) / kNumObjectsPerPage;
  for (int i = 0; i < kNumObjects; i++) {
    HeapObject* object =
        compaction_space->AllocateRawUnaligned(kMaxRegularHeapObjectSize)
            .ToObjectChecked();
    heap->CreateFillerObjectAt(object->address(), kMaxRegularHeapObjectSize,
                               ClearRecordedSlots::kNo);
  }
  int pages_in_old_space = old_space->CountTotalPages();
  int pages_in_compaction_space = compaction_space->CountTotalPages();
  EXPECT_EQ(kExpectedPages, pages_in_compaction_space);
  old_space->MergeCompactionSpace(compaction_space);
  EXPECT_EQ(pages_in_old_space + pages_in_compaction_space,
            old_space->CountTotalPages());

  delete compaction_space;
}

}  // namespace internal
}  // namespace v8
