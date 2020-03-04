// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/spaces.h"
#include <memory>
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/spaces-inl.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

using SpacesTest = TestWithIsolate;

TEST_F(SpacesTest, CompactionSpaceMerge) {
  Heap* heap = i_isolate()->heap();
  OldSpace* old_space = heap->old_space();
  EXPECT_TRUE(old_space != nullptr);

  CompactionSpace* compaction_space =
      new CompactionSpace(heap, OLD_SPACE, NOT_EXECUTABLE,
                          LocalSpaceKind::kCompactionSpaceForMarkCompact);
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
    HeapObject object =
        compaction_space->AllocateRawUnaligned(kMaxRegularHeapObjectSize)
            .ToObjectChecked();
    heap->CreateFillerObjectAt(object.address(), kMaxRegularHeapObjectSize,
                               ClearRecordedSlots::kNo);
  }
  int pages_in_old_space = old_space->CountTotalPages();
  int pages_in_compaction_space = compaction_space->CountTotalPages();
  EXPECT_EQ(kExpectedPages, pages_in_compaction_space);
  old_space->MergeLocalSpace(compaction_space);
  EXPECT_EQ(pages_in_old_space + pages_in_compaction_space,
            old_space->CountTotalPages());

  delete compaction_space;
}

class OffThreadAllocationThread final : public base::Thread {
 public:
  explicit OffThreadAllocationThread(Heap* heap)
      : Thread(Options("OffThreadAllocationThread")), heap_(heap) {}
  void Run() override {
    off_thread_space_ = std::make_unique<OffThreadSpace>(heap_);
    EXPECT_TRUE(off_thread_space_ != nullptr);

    // Cannot loop until "Available()" since we initially have 0 bytes available
    // and would thus neither grow, nor be able to allocate an object.
    const int kNumObjects = 10;
    const int kNumObjectsPerPage =
        off_thread_space_->AreaSize() / kMaxRegularHeapObjectSize;
    const int kExpectedPages =
        (kNumObjects + kNumObjectsPerPage - 1) / kNumObjectsPerPage;
    for (int i = 0; i < kNumObjects; i++) {
      HeapObject object =
          off_thread_space_->AllocateRawUnaligned(kMaxRegularHeapObjectSize)
              .ToObjectChecked();
      heap_->CreateFillerObjectAt(object.address(), kMaxRegularHeapObjectSize,
                                  ClearRecordedSlots::kNo);
    }
    int pages_in_off_thread_space = off_thread_space_->CountTotalPages();
    EXPECT_EQ(kExpectedPages, pages_in_off_thread_space);
  }

  OffThreadSpace* space() { return off_thread_space_.get(); }

 private:
  Heap* heap_;
  std::unique_ptr<OffThreadSpace> off_thread_space_;
};

TEST_F(SpacesTest, OffThreadSpaceAllocate) {
  Heap* heap = i_isolate()->heap();

  static const int kNumThreads = 10;
  std::unique_ptr<OffThreadAllocationThread> threads[10];
  for (int i = 0; i < kNumThreads; ++i) {
    threads[i] = std::make_unique<OffThreadAllocationThread>(heap);
  }
  for (int i = 0; i < kNumThreads; ++i) {
    CHECK(threads[i]->Start());
  }
  for (int i = 0; i < kNumThreads; ++i) {
    threads[i]->Join();
  }
}

TEST_F(SpacesTest, OffThreadSpaceMerge) {
  Heap* heap = i_isolate()->heap();
  OldSpace* old_space = heap->old_space();
  EXPECT_TRUE(old_space != nullptr);

  static const int kNumThreads = 10;
  std::unique_ptr<OffThreadAllocationThread> threads[10];
  for (int i = 0; i < kNumThreads; ++i) {
    threads[i] = std::make_unique<OffThreadAllocationThread>(heap);
  }
  for (int i = 0; i < kNumThreads; ++i) {
    CHECK(threads[i]->Start());
  }
  for (int i = 0; i < kNumThreads; ++i) {
    threads[i]->Join();
  }

  int pages_in_old_space = old_space->CountTotalPages();

  int expected_merged_pages = 0;
  for (int i = 0; i < kNumThreads; ++i) {
    int pages_in_off_thread_space = threads[i]->space()->CountTotalPages();

    old_space->MergeLocalSpace(threads[i]->space());
    expected_merged_pages += pages_in_off_thread_space;
  }

  EXPECT_EQ(pages_in_old_space + expected_merged_pages,
            old_space->CountTotalPages());
}

class LargeOffThreadAllocationThread final : public base::Thread {
 public:
  explicit LargeOffThreadAllocationThread(Heap* heap)
      : Thread(Options("LargeOffThreadAllocationThread")), heap_(heap) {}
  void Run() override {
    off_thread_lo_space_ = std::make_unique<OffThreadLargeObjectSpace>(heap_);
    EXPECT_TRUE(off_thread_lo_space_ != nullptr);

    const int kNumObjects = 10;
    const int kExpectedPages = kNumObjects;
    for (int i = 0; i < kNumObjects; i++) {
      HeapObject object =
          off_thread_lo_space_
              ->AllocateRaw(kMaxRegularHeapObjectSize + kTaggedSize)
              .ToObjectChecked();
      heap_->CreateFillerObjectAt(object.address(),
                                  kMaxRegularHeapObjectSize + kTaggedSize,
                                  ClearRecordedSlots::kNo);
    }
    int pages_in_off_thread_space = off_thread_lo_space_->PageCount();
    EXPECT_EQ(kExpectedPages, pages_in_off_thread_space);
  }

  OffThreadLargeObjectSpace* space() { return off_thread_lo_space_.get(); }

 private:
  Heap* heap_;
  std::unique_ptr<OffThreadLargeObjectSpace> off_thread_lo_space_;
};

TEST_F(SpacesTest, OffThreadLargeObjectSpaceAllocate) {
  Heap* heap = i_isolate()->heap();

  static const int kNumThreads = 10;
  std::unique_ptr<LargeOffThreadAllocationThread> threads[10];
  for (int i = 0; i < kNumThreads; ++i) {
    threads[i] = std::make_unique<LargeOffThreadAllocationThread>(heap);
  }
  for (int i = 0; i < kNumThreads; ++i) {
    CHECK(threads[i]->Start());
  }
  for (int i = 0; i < kNumThreads; ++i) {
    threads[i]->Join();
  }
}

TEST_F(SpacesTest, OffThreadLargeObjectSpaceMerge) {
  Heap* heap = i_isolate()->heap();
  OldLargeObjectSpace* lo_space = heap->lo_space();
  EXPECT_TRUE(lo_space != nullptr);

  static const int kNumThreads = 10;
  std::unique_ptr<LargeOffThreadAllocationThread> threads[10];
  for (int i = 0; i < kNumThreads; ++i) {
    threads[i] = std::make_unique<LargeOffThreadAllocationThread>(heap);
  }
  for (int i = 0; i < kNumThreads; ++i) {
    CHECK(threads[i]->Start());
  }
  for (int i = 0; i < kNumThreads; ++i) {
    threads[i]->Join();
  }

  int pages_in_old_space = lo_space->PageCount();

  int expected_merged_pages = 0;
  for (int i = 0; i < kNumThreads; ++i) {
    int pages_in_off_thread_space = threads[i]->space()->PageCount();

    lo_space->MergeOffThreadSpace(threads[i]->space());
    expected_merged_pages += pages_in_off_thread_space;
  }

  EXPECT_EQ(pages_in_old_space + expected_merged_pages, lo_space->PageCount());
}

TEST_F(SpacesTest, WriteBarrierFromHeapObject) {
  constexpr Address address1 = Page::kPageSize;
  HeapObject object1 = HeapObject::unchecked_cast(Object(address1));
  MemoryChunk* chunk1 = MemoryChunk::FromHeapObject(object1);
  heap_internals::MemoryChunk* slim_chunk1 =
      heap_internals::MemoryChunk::FromHeapObject(object1);
  EXPECT_EQ(static_cast<void*>(chunk1), static_cast<void*>(slim_chunk1));
  constexpr Address address2 = 2 * Page::kPageSize - 1;
  HeapObject object2 = HeapObject::unchecked_cast(Object(address2));
  MemoryChunk* chunk2 = MemoryChunk::FromHeapObject(object2);
  heap_internals::MemoryChunk* slim_chunk2 =
      heap_internals::MemoryChunk::FromHeapObject(object2);
  EXPECT_EQ(static_cast<void*>(chunk2), static_cast<void*>(slim_chunk2));
}

TEST_F(SpacesTest, WriteBarrierIsMarking) {
  const size_t kSizeOfMemoryChunk = sizeof(MemoryChunk);
  char memory[kSizeOfMemoryChunk];
  memset(&memory, 0, kSizeOfMemoryChunk);
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

TEST_F(SpacesTest, WriteBarrierInYoungGenerationToSpace) {
  const size_t kSizeOfMemoryChunk = sizeof(MemoryChunk);
  char memory[kSizeOfMemoryChunk];
  memset(&memory, 0, kSizeOfMemoryChunk);
  MemoryChunk* chunk = reinterpret_cast<MemoryChunk*>(&memory);
  heap_internals::MemoryChunk* slim_chunk =
      reinterpret_cast<heap_internals::MemoryChunk*>(&memory);
  EXPECT_FALSE(chunk->InYoungGeneration());
  EXPECT_FALSE(slim_chunk->InYoungGeneration());
  chunk->SetFlag(MemoryChunk::TO_PAGE);
  EXPECT_TRUE(chunk->InYoungGeneration());
  EXPECT_TRUE(slim_chunk->InYoungGeneration());
  chunk->ClearFlag(MemoryChunk::TO_PAGE);
  EXPECT_FALSE(chunk->InYoungGeneration());
  EXPECT_FALSE(slim_chunk->InYoungGeneration());
}

TEST_F(SpacesTest, WriteBarrierInYoungGenerationFromSpace) {
  const size_t kSizeOfMemoryChunk = sizeof(MemoryChunk);
  char memory[kSizeOfMemoryChunk];
  memset(&memory, 0, kSizeOfMemoryChunk);
  MemoryChunk* chunk = reinterpret_cast<MemoryChunk*>(&memory);
  heap_internals::MemoryChunk* slim_chunk =
      reinterpret_cast<heap_internals::MemoryChunk*>(&memory);
  EXPECT_FALSE(chunk->InYoungGeneration());
  EXPECT_FALSE(slim_chunk->InYoungGeneration());
  chunk->SetFlag(MemoryChunk::FROM_PAGE);
  EXPECT_TRUE(chunk->InYoungGeneration());
  EXPECT_TRUE(slim_chunk->InYoungGeneration());
  chunk->ClearFlag(MemoryChunk::FROM_PAGE);
  EXPECT_FALSE(chunk->InYoungGeneration());
  EXPECT_FALSE(slim_chunk->InYoungGeneration());
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

// Tests that FreeListMany::SelectFreeListCategoryType returns what it should.
TEST_F(SpacesTest, FreeListManySelectFreeListCategoryType) {
  FreeListMany free_list;

  // Testing that all sizes below 256 bytes get assigned the correct category
  for (size_t size = 0; size <= FreeListMany::kPreciseCategoryMaxSize; size++) {
    FreeListCategoryType cat = free_list.SelectFreeListCategoryType(size);
    if (cat == 0) {
      // If cat == 0, then we make sure that |size| doesn't fit in the 2nd
      // category.
      EXPECT_LT(size, free_list.categories_min[1]);
    } else {
      // Otherwise, size should fit in |cat|, but not in |cat+1|.
      EXPECT_LE(free_list.categories_min[cat], size);
      EXPECT_LT(size, free_list.categories_min[cat + 1]);
    }
  }

  // Testing every size above 256 would take long time, so test only some
  // "interesting cases": picking some number in the middle of the categories,
  // as well as at the categories' bounds.
  for (int cat = kFirstCategory + 1; cat <= free_list.last_category_; cat++) {
    std::vector<size_t> sizes;
    // Adding size less than this category's minimum
    sizes.push_back(free_list.categories_min[cat] - 8);
    // Adding size equal to this category's minimum
    sizes.push_back(free_list.categories_min[cat]);
    // Adding size greater than this category's minimum
    sizes.push_back(free_list.categories_min[cat] + 8);
    // Adding size between this category's minimum and the next category
    if (cat != free_list.last_category_) {
      sizes.push_back(
          (free_list.categories_min[cat] + free_list.categories_min[cat + 1]) /
          2);
    }

    for (size_t size : sizes) {
      FreeListCategoryType cat = free_list.SelectFreeListCategoryType(size);
      if (cat == free_list.last_category_) {
        // If cat == last_category, then we make sure that |size| indeeds fits
        // in the last category.
        EXPECT_LE(free_list.categories_min[cat], size);
      } else {
        // Otherwise, size should fit in |cat|, but not in |cat+1|.
        EXPECT_LE(free_list.categories_min[cat], size);
        EXPECT_LT(size, free_list.categories_min[cat + 1]);
      }
    }
  }
}

// Tests that FreeListMany::GuaranteedAllocatable returns what it should.
TEST_F(SpacesTest, FreeListManyGuaranteedAllocatable) {
  FreeListMany free_list;

  for (int cat = kFirstCategory; cat < free_list.last_category_; cat++) {
    std::vector<size_t> sizes;
    // Adding size less than this category's minimum
    sizes.push_back(free_list.categories_min[cat] - 8);
    // Adding size equal to this category's minimum
    sizes.push_back(free_list.categories_min[cat]);
    // Adding size greater than this category's minimum
    sizes.push_back(free_list.categories_min[cat] + 8);
    if (cat != free_list.last_category_) {
      // Adding size between this category's minimum and the next category
      sizes.push_back(
          (free_list.categories_min[cat] + free_list.categories_min[cat + 1]) /
          2);
    }

    for (size_t size : sizes) {
      FreeListCategoryType cat_free =
          free_list.SelectFreeListCategoryType(size);
      size_t guaranteed_allocatable = free_list.GuaranteedAllocatable(size);
      if (cat_free == free_list.last_category_) {
        // If |cat_free| == last_category, then guaranteed_allocatable must
        // return the last category, because when allocating, the last category
        // is searched entirely.
        EXPECT_EQ(free_list.SelectFreeListCategoryType(guaranteed_allocatable),
                  free_list.last_category_);
      } else if (size < free_list.categories_min[0]) {
        // If size < free_list.categories_min[0], then the bytes are wasted, and
        // guaranteed_allocatable should return 0.
        EXPECT_EQ(guaranteed_allocatable, 0ul);
      } else {
        // Otherwise, |guaranteed_allocatable| is equal to the minimum of
        // |size|'s category (|cat_free|);
        EXPECT_EQ(free_list.categories_min[cat_free], guaranteed_allocatable);
      }
    }
  }
}

// Tests that
// FreeListManyCachedFastPath::SelectFastAllocationFreeListCategoryType returns
// what it should.
TEST_F(SpacesTest,
       FreeListManyCachedFastPathSelectFastAllocationFreeListCategoryType) {
  FreeListManyCachedFastPath free_list;

  for (int cat = kFirstCategory; cat <= free_list.last_category_; cat++) {
    std::vector<size_t> sizes;
    // Adding size less than this category's minimum
    sizes.push_back(free_list.categories_min[cat] - 8);
    // Adding size equal to this category's minimum
    sizes.push_back(free_list.categories_min[cat]);
    // Adding size greater than this category's minimum
    sizes.push_back(free_list.categories_min[cat] + 8);
    // Adding size between this category's minimum and the next category
    if (cat != free_list.last_category_) {
      sizes.push_back(
          (free_list.categories_min[cat] + free_list.categories_min[cat + 1]) /
          2);
    }

    for (size_t size : sizes) {
      FreeListCategoryType cat =
          free_list.SelectFastAllocationFreeListCategoryType(size);
      if (size <= FreeListManyCachedFastPath::kTinyObjectMaxSize) {
        // For tiny objects, the first category of the fast path should be
        // chosen.
        EXPECT_TRUE(cat == FreeListManyCachedFastPath::kFastPathFirstCategory);
      } else if (size >= free_list.categories_min[free_list.last_category_] -
                             FreeListManyCachedFastPath::kFastPathOffset) {
        // For objects close to the minimum of the last category, the last
        // category is chosen.
        EXPECT_EQ(cat, free_list.last_category_);
      } else {
        // For other objects, the chosen category must satisfy that its minimum
        // is at least |size|+1.85k.
        EXPECT_GE(free_list.categories_min[cat],
                  size + FreeListManyCachedFastPath::kFastPathOffset);
        // And the smaller categoriy's minimum is less than |size|+1.85k
        // (otherwise it would have been chosen instead).
        EXPECT_LT(free_list.categories_min[cat - 1],
                  size + FreeListManyCachedFastPath::kFastPathOffset);
      }
    }
  }
}

}  // namespace internal
}  // namespace v8
