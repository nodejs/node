// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/isolate.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

typedef TestWithIsolate SpacesTest;

TEST_F(SpacesTest, CompactionSpaceMerge) {
  Heap* heap = i_isolate()->heap();
  OldSpace* old_space = heap->old_space();
  EXPECT_TRUE(old_space != nullptr);

  CompactionSpace* compaction_space =
      new CompactionSpace(heap, OLD_SPACE, NOT_EXECUTABLE);
  EXPECT_TRUE(compaction_space != nullptr);

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

TEST_F(SpacesTest, WriteBarrierFromHeapObject) {
  constexpr Address address1 = Page::kPageSize;
  HeapObject* object1 = reinterpret_cast<HeapObject*>(address1);
  MemoryChunk* chunk1 = MemoryChunk::FromHeapObject(object1);
  heap_internals::MemoryChunk* slim_chunk1 =
      heap_internals::MemoryChunk::FromHeapObject(object1);
  EXPECT_EQ(static_cast<void*>(chunk1), static_cast<void*>(slim_chunk1));
  constexpr Address address2 = 2 * Page::kPageSize - 1;
  HeapObject* object2 = reinterpret_cast<HeapObject*>(address2);
  MemoryChunk* chunk2 = MemoryChunk::FromHeapObject(object2);
  heap_internals::MemoryChunk* slim_chunk2 =
      heap_internals::MemoryChunk::FromHeapObject(object2);
  EXPECT_EQ(static_cast<void*>(chunk2), static_cast<void*>(slim_chunk2));
}

TEST_F(SpacesTest, WriteBarrierIsMarking) {
  char memory[256];
  memset(&memory, 0, sizeof(memory));
  MemoryChunk* chunk = reinterpret_cast<MemoryChunk*>(&memory);
  heap_internals::MemoryChunk* slim_chunk =
      reinterpret_cast<heap_internals::MemoryChunk*>(&memory);
  EXPECT_FALSE(chunk->IsFlagSet(MemoryChunk::INCREMENTAL_MARKING));
  EXPECT_FALSE(slim_chunk->IsMarking());
  chunk->SetFlag(MemoryChunk::INCREMENTAL_MARKING);
  EXPECT_TRUE(chunk->IsFlagSet(MemoryChunk::INCREMENTAL_MARKING));
  EXPECT_TRUE(slim_chunk->IsMarking());
  chunk->ClearFlag(MemoryChunk::INCREMENTAL_MARKING);
  EXPECT_FALSE(chunk->IsFlagSet(MemoryChunk::INCREMENTAL_MARKING));
  EXPECT_FALSE(slim_chunk->IsMarking());
}

TEST_F(SpacesTest, WriteBarrierInNewSpaceToSpace) {
  char memory[256];
  memset(&memory, 0, sizeof(memory));
  MemoryChunk* chunk = reinterpret_cast<MemoryChunk*>(&memory);
  heap_internals::MemoryChunk* slim_chunk =
      reinterpret_cast<heap_internals::MemoryChunk*>(&memory);
  EXPECT_FALSE(chunk->InNewSpace());
  EXPECT_FALSE(slim_chunk->InNewSpace());
  chunk->SetFlag(MemoryChunk::IN_TO_SPACE);
  EXPECT_TRUE(chunk->InNewSpace());
  EXPECT_TRUE(slim_chunk->InNewSpace());
  chunk->ClearFlag(MemoryChunk::IN_TO_SPACE);
  EXPECT_FALSE(chunk->InNewSpace());
  EXPECT_FALSE(slim_chunk->InNewSpace());
}

TEST_F(SpacesTest, WriteBarrierInNewSpaceFromSpace) {
  char memory[256];
  memset(&memory, 0, sizeof(memory));
  MemoryChunk* chunk = reinterpret_cast<MemoryChunk*>(&memory);
  heap_internals::MemoryChunk* slim_chunk =
      reinterpret_cast<heap_internals::MemoryChunk*>(&memory);
  EXPECT_FALSE(chunk->InNewSpace());
  EXPECT_FALSE(slim_chunk->InNewSpace());
  chunk->SetFlag(MemoryChunk::IN_FROM_SPACE);
  EXPECT_TRUE(chunk->InNewSpace());
  EXPECT_TRUE(slim_chunk->InNewSpace());
  chunk->ClearFlag(MemoryChunk::IN_FROM_SPACE);
  EXPECT_FALSE(chunk->InNewSpace());
  EXPECT_FALSE(slim_chunk->InNewSpace());
}

TEST_F(SpacesTest, CodeRangeAddressReuse) {
  CodeRangeAddressHint hint;
  // Create code ranges.
  Address code_range1 = hint.GetAddressHint(100);
  Address code_range2 = hint.GetAddressHint(200);
  Address code_range3 = hint.GetAddressHint(100);

  // Since the addresses are random, we cannot check that they are different.

  // Free two code ranges.
  hint.NotifyFreedCodeRange(code_range1, 100);
  hint.NotifyFreedCodeRange(code_range2, 200);

  // The next two code ranges should reuse the freed addresses.
  Address code_range4 = hint.GetAddressHint(100);
  EXPECT_EQ(code_range4, code_range1);
  Address code_range5 = hint.GetAddressHint(200);
  EXPECT_EQ(code_range5, code_range2);

  // Free the third code range and check address reuse.
  hint.NotifyFreedCodeRange(code_range3, 100);
  Address code_range6 = hint.GetAddressHint(100);
  EXPECT_EQ(code_range6, code_range3);
}

}  // namespace internal
}  // namespace v8
