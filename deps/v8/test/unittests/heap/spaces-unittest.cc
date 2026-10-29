// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/spaces.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "include/v8-platform.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/base/region-allocator.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/handles/handles.h"
#include "src/heap/allocation-result.h"
#include "src/heap/code-range.h"
#include "src/heap/free-list.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/heap.h"
#include "src/heap/large-spaces.h"
#include "src/heap/main-allocator-inl.h"
#include "src/heap/main-allocator.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-pool.h"
#include "src/heap/mutable-page.h"
#include "src/heap/new-spaces.h"
#include "src/heap/paged-spaces.h"
#include "src/heap/read-only-spaces.h"
#include "src/heap/safepoint.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/sweeper.h"
#include "src/init/isolate-group.h"
#include "src/objects/objects-inl.h"
#include "src/utils/allocation.h"
#include "src/utils/ostreams.h"
#include "test/common/flag-utils.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

// The two scopes below must live in v8::internal::heap because Heap and
// MemoryAllocator befriend them under that name (see heap.h and
// memory-allocator.h).
namespace heap {

// Temporarily sets a given allocator in an isolate.
class V8_NODISCARD TestMemoryAllocatorScope {
 public:
  TestMemoryAllocatorScope(Isolate* isolate, size_t max_capacity,
                           PageAllocator* page_allocator = nullptr)
      : isolate_(isolate),
        old_allocator_(std::move(isolate->heap()->memory_allocator_)) {
    // Save the code pages for restoring them later on because the constructor
    // of MemoryAllocator will change them.
    isolate->GetCodePages()->swap(code_pages_);
    PageAllocator* effective_allocator =
        page_allocator != nullptr ? page_allocator : isolate->page_allocator();
    isolate->heap()->memory_allocator_ = std::make_unique<MemoryAllocator>(
        isolate, effective_allocator, effective_allocator,
        isolate->isolate_group()->memory_pool(), max_capacity);
    if (page_allocator != nullptr) {
      isolate->heap()->memory_allocator_->data_page_allocator_ = page_allocator;
    }
  }

  MemoryAllocator* allocator() { return isolate_->heap()->memory_allocator(); }

  ~TestMemoryAllocatorScope() {
    isolate_->heap()->memory_allocator()->ReleasePooledChunksImmediately();
    isolate_->heap()->memory_allocator()->TearDown();
    isolate_->heap()->memory_allocator_.swap(old_allocator_);
    isolate_->GetCodePages()->swap(code_pages_);
  }

  TestMemoryAllocatorScope(const TestMemoryAllocatorScope&) = delete;
  TestMemoryAllocatorScope& operator=(const TestMemoryAllocatorScope&) = delete;

 private:
  Isolate* isolate_;
  std::unique_ptr<MemoryAllocator> old_allocator_;
  std::vector<MemoryRange> code_pages_;
};

// Temporarily sets a given code page allocator in an isolate.
class V8_NODISCARD TestCodePageAllocatorScope {
 public:
  TestCodePageAllocatorScope(Isolate* isolate,
                             v8::PageAllocator* code_page_allocator)
      : isolate_(isolate),
        old_code_page_allocator_(
            isolate->heap()->memory_allocator()->code_page_allocator()) {
    isolate->heap()->memory_allocator()->code_page_allocator_ =
        code_page_allocator;
  }

  ~TestCodePageAllocatorScope() {
    isolate_->heap()->memory_allocator()->code_page_allocator_ =
        old_code_page_allocator_;
  }
  TestCodePageAllocatorScope(const TestCodePageAllocatorScope&) = delete;
  TestCodePageAllocatorScope& operator=(const TestCodePageAllocatorScope&) =
      delete;

 private:
  Isolate* isolate_;
  v8::PageAllocator* old_code_page_allocator_;
};

}  // namespace heap

using heap::TestCodePageAllocatorScope;
using heap::TestMemoryAllocatorScope;

namespace {

Tagged<HeapObject> AllocateUnaligned(MainAllocator* allocator,
                                     SpaceWithLinearArea* space, int size) {
  AllocationResult allocation =
      allocator->AllocateRaw(SafeHeapObjectSize(size), kTaggedAligned,
                             AllocationOrigin::kRuntime, AllocationHint());
  CHECK(!allocation.IsFailure());
  Tagged<HeapObject> filler;
  CHECK(allocation.To(&filler));
  space->heap()->CreateFillerObjectAt(filler.address(), size);
  return filler;
}

Tagged<HeapObject> AllocateUnaligned(OldLargeObjectSpace* allocator,
                                     OldLargeObjectSpace* space, int size) {
  AllocationResult allocation = allocator->AllocateRaw(
      space->heap()->main_thread_local_heap(), size, AllocationHint());
  CHECK(!allocation.IsFailure());
  Tagged<HeapObject> filler;
  CHECK(allocation.To(&filler));
  space->heap()->CreateFillerObjectAt(filler.address(), size);
  return filler;
}

class Observer : public AllocationObserver {
 public:
  explicit Observer(intptr_t step_size)
      : AllocationObserver(step_size), count_(0) {}

  void Step(int bytes_allocated, Address addr, size_t) override { count_++; }

  int count() const { return count_; }

 private:
  int count_;
};

template <typename T, typename A>
void TestAllocationObserver(Isolate* i_isolate, T* space, A* allocator) {
  Observer observer1(128);
  i_isolate->heap()->FreeMainThreadLinearAllocationAreas();
  allocator->AddAllocationObserver(&observer1);

  // The observer should not get notified if we have only allocated less than
  // 128 bytes.
  AllocateUnaligned(allocator, space, 64);
  CHECK_EQ(observer1.count(), 0);

  // The observer should get called when we have allocated exactly 128 bytes.
  AllocateUnaligned(allocator, space, 64);
  CHECK_EQ(observer1.count(), 1);

  // Another >128 bytes should get another notification.
  AllocateUnaligned(allocator, space, 136);
  CHECK_EQ(observer1.count(), 2);

  // Allocating a large object should get only one notification.
  AllocateUnaligned(allocator, space, 1024);
  CHECK_EQ(observer1.count(), 3);

  // Allocating another 2048 bytes in small objects should get 16
  // notifications.
  for (int i = 0; i < 64; ++i) {
    AllocateUnaligned(allocator, space, 32);
  }
  CHECK_EQ(observer1.count(), 19);

  // Multiple observers should work.
  Observer observer2(96);
  i_isolate->heap()->FreeMainThreadLinearAllocationAreas();
  allocator->AddAllocationObserver(&observer2);

  AllocateUnaligned(allocator, space, 2048);
  CHECK_EQ(observer1.count(), 20);
  CHECK_EQ(observer2.count(), 1);

  AllocateUnaligned(allocator, space, 104);
  CHECK_EQ(observer1.count(), 20);
  CHECK_EQ(observer2.count(), 2);

  // Callback should stop getting called after an observer is removed.
  allocator->RemoveAllocationObserver(&observer1);

  AllocateUnaligned(allocator, space, 384);
  CHECK_EQ(observer1.count(), 20);  // no more notifications.
  CHECK_EQ(observer2.count(), 3);   // this one is still active.

  // Ensure that PauseInlineAllocationObserversScope work correctly.
  AllocateUnaligned(allocator, space, 48);
  CHECK_EQ(observer2.count(), 3);
  {
    i_isolate->heap()->FreeMainThreadLinearAllocationAreas();
    PauseAllocationObserversScope pause_observers(i_isolate->heap());
    CHECK_EQ(observer2.count(), 3);
    AllocateUnaligned(allocator, space, 384);
    CHECK_EQ(observer2.count(), 3);
    i_isolate->heap()->FreeMainThreadLinearAllocationAreas();
  }
  CHECK_EQ(observer2.count(), 3);
  // Coupled with the 48 bytes allocated before the pause, another 48 bytes
  // allocated here should trigger a notification.
  AllocateUnaligned(allocator, space, 48);
  CHECK_EQ(observer2.count(), 4);

  allocator->RemoveAllocationObserver(&observer2);
  AllocateUnaligned(allocator, space, 384);
  CHECK_EQ(observer1.count(), 20);
  CHECK_EQ(observer2.count(), 4);
}

void VerifyMemoryChunk(Isolate* isolate, v8::PageAllocator* code_page_allocator,
                       size_t area_size, Executability executable,
                       LargeObjectSpace* space) {
  Heap* heap = isolate->heap();
  TestMemoryAllocatorScope test_allocator_scope(isolate, heap->MaxReserved());
  MemoryAllocator* memory_allocator = test_allocator_scope.allocator();
  TestCodePageAllocatorScope test_code_page_allocator_scope(
      isolate, code_page_allocator);

  v8::PageAllocator* page_allocator =
      memory_allocator->page_allocator(space->identity());

  size_t allocatable_memory_area_offset =
      MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(space->identity());

  MutablePage* memory_chunk = memory_allocator->AllocateLargePage(
      space, area_size, executable, AllocationHint());
  ASSERT_NE(nullptr, memory_chunk);
  size_t reserved_size =
      (executable == EXECUTABLE)
          ? RoundUp(allocatable_memory_area_offset +
                        RoundUp(area_size, page_allocator->CommitPageSize()),
                    page_allocator->CommitPageSize())
          : RoundUp(allocatable_memory_area_offset + area_size,
                    page_allocator->CommitPageSize());
  CHECK_EQ(memory_chunk->size(), reserved_size);
  CHECK_LT(memory_chunk->area_start(),
           memory_chunk->ChunkAddress() + memory_chunk->size());
  CHECK_LE(memory_chunk->area_end(),
           memory_chunk->ChunkAddress() + memory_chunk->size());
  CHECK_EQ(static_cast<size_t>(memory_chunk->area_size()), area_size);

  memory_allocator->Free(MemoryAllocator::FreeMode::kImmediately, memory_chunk);
}

unsigned int PseudorandomAreaSize() {
  static uint32_t lo = 2345;
  lo = 18273 * (lo & 0xFFFFF) + (lo >> 16);
  return lo & 0xFFFFF;
}

template <typename TMixin>
class WithSmallHeapFlagsMixin : public TMixin {
 public:
  WithSmallHeapFlagsMixin() {
    v8_flags.max_heap_size = 20;
    // These tests use their own old/large object space, which confuses the
    // incremental marker.
    v8_flags.incremental_marking = false;
    // These tests don't expect GCs caused by concurrent allocations in the
    // background thread.
    v8_flags.stress_concurrent_allocation = false;
  }

 private:
  SaveFlags save_flags_;
};

// PageAllocator that always fails.
class FailingPageAllocator : public v8::PageAllocator {
 public:
  size_t AllocatePageSize() override { return 1024; }
  size_t CommitPageSize() override { return 1024; }
  void SetRandomMmapSeed(int64_t seed) override {}
  void* GetRandomMmapAddr() override { return nullptr; }
  void* AllocatePages(void* address, size_t length, size_t alignment,
                      Permission permissions) override {
    return nullptr;
  }
  bool FreePages(void* address, size_t length) override { return false; }
  bool ReleasePages(void* address, size_t length, size_t new_length) override {
    return false;
  }
  bool SetPermissions(void* address, size_t length,
                      Permission permissions) override {
    return false;
  }
  bool RecommitPages(void* address, size_t length,
                     Permission permissions) override {
    return false;
  }
  bool DecommitPages(void* address, size_t length) override { return false; }
  bool SealPages(void* address, size_t length) override { return false; }
};

// ReadOnlySpace cannot be torn down by a destructor because the destructor
// cannot take an argument. Since these tests create ReadOnlySpaces not attached
// to the Heap directly, they need to be destroyed to ensure the
// MemoryAllocator's stats are all 0 at exit.
class V8_NODISCARD ReadOnlySpaceScope {
 public:
  explicit ReadOnlySpaceScope(Heap* heap) : heap_(heap), ro_space_(heap) {}
  ~ReadOnlySpaceScope() { ro_space_.TearDown(heap_->memory_allocator()); }

  ReadOnlySpace* space() { return &ro_space_; }

 private:
  Heap* heap_;
  ReadOnlySpace ro_space_;
};

}  // namespace

using SpacesTest = TestWithHeapInternals;

using SpacesTestWithSmallHeap =                       //
    WithHeapInternals<                                //
        WithInternalIsolateMixin<                     //
            WithIsolateScopeMixin<                    //
                WithIsolateMixin<                     //
                    WithCppHeap<                      //
                        WithDefaultPlatformMixin<     //
                            WithSmallHeapFlagsMixin<  //
                                ::testing::Test>>>>>>>;

TEST_F(SpacesTest, CompactionSpaceMerge) {
  Heap* heap = i_isolate()->heap();
  OldSpace* old_space = heap->old_space();
  ASSERT_NE(nullptr, old_space);

  heap->SetGCState(Heap::MARK_COMPACT);

  auto compaction_space = std::make_unique<CompactionSpace>(
      heap, OLD_SPACE, NOT_EXECUTABLE,
      CompactionSpaceKind::kCompactionSpaceForMarkCompact,
      CompactionSpace::DestinationHeap::kSameHeap);
  MainAllocator allocator(heap, compaction_space.get(), MainAllocator::kInGC);

  for (NormalPage* p : *old_space) {
    // Unlink free lists from the main space to avoid reusing the memory for
    // compaction spaces.
    old_space->free_list()->EvictFreeListItems(p);
  }

  // Cannot loop until "Available()" since we initially have 0 bytes available
  // and would thus neither grow, nor be able to allocate an object.
  const int kNumObjects = 10;
  const int kNumObjectsPerPage =
      compaction_space->AreaSize() / kMaxRegularHeapObjectSize;
  const int kExpectedPages =
      (kNumObjects + kNumObjectsPerPage - 1) / kNumObjectsPerPage;
  for (int i = 0; i < kNumObjects; i++) {
    Tagged<HeapObject> object =
        allocator
            .AllocateRaw(SafeHeapObjectSize(kMaxRegularHeapObjectSize),
                         kTaggedAligned, AllocationOrigin::kGC,
                         AllocationHint())
            .ToObjectChecked();
    heap->CreateFillerObjectAt(object.address(), kMaxRegularHeapObjectSize);
  }
  int pages_in_old_space = old_space->CountTotalPages();
  int pages_in_compaction_space = compaction_space->CountTotalPages();
  EXPECT_EQ(kExpectedPages, pages_in_compaction_space);
  allocator.FreeLinearAllocationArea();
  old_space->MergeCompactionSpace(compaction_space.get());
  EXPECT_EQ(pages_in_old_space + pages_in_compaction_space,
            old_space->CountTotalPages());

  compaction_space.reset();

  heap->SetGCState(Heap::NOT_IN_GC);
}

TEST_F(SpacesTest, WriteBarriers) {
  // Test allocates a real page in OLD_SPACE to check various flag combinaton.
  Heap* heap = i_isolate()->heap();
  OldSpace* old_space = heap->old_space();
  ASSERT_NE(nullptr, old_space);

  for (NormalPage* p : *old_space) {
    // Unlink free lists from the main space to avoid reusing the memory for
    // compaction spaces.
    old_space->free_list()->EvictFreeListItems(p);
  }

  heap->SetGCState(Heap::MARK_COMPACT);
  {
    auto compaction_space = std::make_unique<CompactionSpace>(
        heap, OLD_SPACE, NOT_EXECUTABLE,
        CompactionSpaceKind::kCompactionSpaceForMarkCompact,
        CompactionSpace::DestinationHeap::kSameHeap);
    EXPECT_TRUE(compaction_space);
    MainAllocator allocator(heap, compaction_space.get(), MainAllocator::kInGC);

    Tagged<HeapObject> object =
        allocator
            .AllocateRaw(SafeHeapObjectSize(kMaxRegularHeapObjectSize),
                         kTaggedAligned, AllocationOrigin::kGC,
                         AllocationHint())
            .ToObjectChecked();
    heap->CreateFillerObjectAt(object.address(), kMaxRegularHeapObjectSize);
    EXPECT_EQ(1, compaction_space->CountTotalPages());

    MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
    MutablePage* metadata = MutablePage::FromHeapObject(i_isolate(), object);

    // Marking states.
    EXPECT_FALSE(chunk->IsMarking());
    metadata->SetFlagNonExecutable(MemoryChunk::INCREMENTAL_MARKING);
    EXPECT_TRUE(chunk->IsMarking());
    metadata->ClearFlagNonExecutable(MemoryChunk::INCREMENTAL_MARKING);
    EXPECT_FALSE(chunk->IsMarking());

    // In young generation for TO space.
    EXPECT_FALSE(chunk->InYoungGeneration());
    metadata->SetFlagNonExecutable(MemoryChunk::TO_PAGE);
    EXPECT_TRUE(chunk->InYoungGeneration());
    metadata->ClearFlagNonExecutable(MemoryChunk::TO_PAGE);
    EXPECT_FALSE(chunk->InYoungGeneration());

    // In young generation for FROM space.
    EXPECT_FALSE(chunk->InYoungGeneration());
    metadata->SetFlagNonExecutable(MemoryChunk::FROM_PAGE);
    EXPECT_TRUE(chunk->InYoungGeneration());
    metadata->ClearFlagNonExecutable(MemoryChunk::FROM_PAGE);
    EXPECT_FALSE(chunk->InYoungGeneration());
  }
  heap->SetGCState(Heap::NOT_IN_GC);
}

TEST_F(SpacesTest, CodeRangeAddressReuse) {
  CodeRangeAddressHint hint;
  const size_t base_alignment = NormalPage::kPageSize;
  // Create code ranges.
  Address code_range1 = hint.GetAddressHint(100, base_alignment);
  CHECK(IsAligned(code_range1, base_alignment));
  Address code_range2 = hint.GetAddressHint(200, base_alignment);
  CHECK(IsAligned(code_range2, base_alignment));
  Address code_range3 = hint.GetAddressHint(100, base_alignment);
  CHECK(IsAligned(code_range3, base_alignment));

  // Since the addresses are random, we cannot check that they are different.

  // Free two code ranges.
  hint.NotifyFreedCodeRange(code_range1, 100);
  hint.NotifyFreedCodeRange(code_range2, 200);

  // The next two code ranges should reuse the freed addresses.
  Address code_range4 = hint.GetAddressHint(100, base_alignment);
  EXPECT_EQ(code_range4, code_range1);
  Address code_range5 = hint.GetAddressHint(200, base_alignment);
  EXPECT_EQ(code_range5, code_range2);

  // Free the third code range and check address reuse.
  hint.NotifyFreedCodeRange(code_range3, 100);
  Address code_range6 = hint.GetAddressHint(100, base_alignment);
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
      FreeListCategoryType selected =
          free_list.SelectFreeListCategoryType(size);
      if (selected == free_list.last_category_) {
        // If selected == last_category, then we make sure that |size| indeeds
        // fits in the last category.
        EXPECT_LE(free_list.categories_min[selected], size);
      } else {
        // Otherwise, size should fit in |selected|, but not in |selected+1|.
        EXPECT_LE(free_list.categories_min[selected], size);
        EXPECT_LT(size, free_list.categories_min[selected + 1]);
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
      FreeListCategoryType selected =
          free_list.SelectFastAllocationFreeListCategoryType(size);
      if (size <= FreeListManyCachedFastPath::kTinyObjectMaxSize) {
        // For tiny objects, the first category of the fast path should be
        // chosen.
        EXPECT_TRUE(selected ==
                    FreeListManyCachedFastPath::kFastPathFirstCategory);
      } else if (size >= free_list.categories_min[free_list.last_category_] -
                             FreeListManyCachedFastPath::kFastPathOffset) {
        // For objects close to the minimum of the last category, the last
        // category is chosen.
        EXPECT_EQ(selected, free_list.last_category_);
      } else {
        // For other objects, the chosen category must satisfy that its minimum
        // is at least |size|+1.85k.
        EXPECT_GE(free_list.categories_min[selected],
                  size + FreeListManyCachedFastPath::kFastPathOffset);
        // And the smaller categoriy's minimum is less than |size|+1.85k
        // (otherwise it would have been chosen instead).
        EXPECT_LT(free_list.categories_min[selected - 1],
                  size + FreeListManyCachedFastPath::kFastPathOffset);
      }
    }
  }
}

TEST_F(SpacesTest, AllocationObserver) {
  if (v8_flags.single_generation) return;
  v8::Context::Scope context_scope(v8::Context::New(v8_isolate()));

  TestAllocationObserver<NewSpace>(
      i_isolate(), i_isolate()->heap()->new_space(),
      i_isolate()->heap()->allocator()->new_space_allocator());
  // Old space is used but the code path is shared for all
  // classes inheriting from PagedSpace.
  TestAllocationObserver<PagedSpace>(
      i_isolate(), i_isolate()->heap()->old_space(),
      i_isolate()->heap()->allocator()->old_space_allocator());
  TestAllocationObserver<OldLargeObjectSpace>(i_isolate(),
                                              i_isolate()->heap()->lo_space(),
                                              i_isolate()->heap()->lo_space());
}

TEST_F(SpacesTest, InlineAllocationObserverCadence) {
  if (v8_flags.single_generation) return;
  v8::Context::Scope context_scope(v8::Context::New(v8_isolate()));

  // Clear out any pre-existing garbage to make the test consistent
  // across snapshot/no-snapshot builds.
  InvokeMajorGC();

  MainAllocator* new_space_allocator =
      i_isolate()->heap()->allocator()->new_space_allocator();

  Observer observer1(512);
  new_space_allocator->AddAllocationObserver(&observer1);
  Observer observer2(576);
  new_space_allocator->AddAllocationObserver(&observer2);

  for (int i = 0; i < 512; ++i) {
    AllocateUnaligned(new_space_allocator, i_isolate()->heap()->new_space(),
                      32);
  }

  new_space_allocator->RemoveAllocationObserver(&observer1);
  new_space_allocator->RemoveAllocationObserver(&observer2);

  CHECK_EQ(observer1.count(), 32);
  CHECK_EQ(observer2.count(), 28);
}

#if V8_ENABLE_SANDBOX
TEST_F(SpacesTest, TrustedSpaceNullPage) {
  // Trusted space should have a reserved, inaccessible area at the start to
  // mitigate (compressed) nullptr dereference bugs.

  v8::Context::Scope context_scope(v8::Context::New(v8_isolate()));

  Address trusted_space_base =
      i_isolate()->isolate_group()->GetTrustedPtrComprCageBase();
  const size_t size_of_reserved_area = 1 * MB;

  // Test that no objects are allocated in the reserved area.
  MainAllocator* trusted_space_allocator =
      i_isolate()->heap()->allocator()->trusted_space_allocator();
  for (int i = 0; i < 64; ++i) {
    Tagged<HeapObject> allocation = AllocateUnaligned(
        trusted_space_allocator, i_isolate()->heap()->trusted_space(), 32);
    CHECK_GE(allocation.address(), trusted_space_base);
    size_t offset = allocation.address() - trusted_space_base;
    CHECK_GT(offset, size_of_reserved_area);
  }

  // Test that the reserved area is inaccessible.
  auto ReadByteAt = [](Address address) {
    return *reinterpret_cast<volatile uint8_t*>(address);
  };
  uint8_t buf = 0;
  EXPECT_DEATH_IF_SUPPORTED(buf += ReadByteAt(trusted_space_base), "");
  EXPECT_DEATH_IF_SUPPORTED(
      buf += ReadByteAt(trusted_space_base + size_of_reserved_area - 1), "");
  // Mostly just to prevent the compiler from optimizing away the memory loads.
  CHECK_EQ(buf, 0);
}
#endif  // V8_ENABLE_SANDBOX

TEST_F(SpacesTest, MutablePage) {
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  IsolateSafepointScope safepoint(heap);

  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();
  size_t area_size;

  for (int i = 0; i < 100; i++) {
    area_size =
        RoundUp(PseudorandomAreaSize(), page_allocator->CommitPageSize());

    const size_t code_range_size = 32 * MB;
#ifdef V8_ENABLE_SANDBOX
    // When the sandbox is enabled, the code assumes that there's only a single
    // code range for easy metadata lookup, so use the process wide code range
    // in this case.
    CodeRange* code_range =
        IsolateGroup::current()->EnsureCodeRange(code_range_size);
    base::BoundedPageAllocator* bounded_page_allocator =
        code_range->page_allocator();
#else
    // With CodeRange.
    bool jitless = isolate->jitless();
    VirtualMemory code_range_reservation(
        page_allocator, code_range_size, PageAllocator::AllocationHint(),
        MemoryChunk::GetAlignmentForAllocation(),
        jitless ? PageAllocator::Permission::kNoAccess
                : PageAllocator::Permission::kNoAccessWillJitLater);

    base::PageInitializationMode page_initialization_mode =
        base::PageInitializationMode::kAllocatedPagesCanBeUninitialized;
    base::PageFreeingMode page_freeing_mode =
        base::PageFreeingMode::kMakeInaccessible;

    if (!jitless) {
      page_initialization_mode = base::PageInitializationMode::kRecommitOnly;
      page_freeing_mode = base::PageFreeingMode::kDiscard;
      void* base = reinterpret_cast<void*>(code_range_reservation.address());
      CHECK(page_allocator->SetPermissions(base, code_range_size,
                                           PageAllocator::kReadWriteExecute));
      CHECK(page_allocator->DiscardSystemPages(base, code_range_size));
    }

    CHECK(code_range_reservation.IsReserved());

    base::BoundedPageAllocator code_page_allocator(
        page_allocator, code_range_reservation.address(),
        code_range_reservation.size(), MemoryChunk::GetAlignmentForAllocation(),
        page_initialization_mode, page_freeing_mode);
    base::BoundedPageAllocator* bounded_page_allocator = &code_page_allocator;
#endif

    VerifyMemoryChunk(isolate, bounded_page_allocator, area_size, EXECUTABLE,
                      heap->code_lo_space());

    VerifyMemoryChunk(isolate, bounded_page_allocator, area_size,
                      NOT_EXECUTABLE, heap->lo_space());
  }
}

TEST_F(SpacesTest, MemoryAllocator) {
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();

  TestMemoryAllocatorScope test_allocator_scope(isolate, heap->MaxReserved());
  MemoryAllocator* memory_allocator = test_allocator_scope.allocator();

  int total_pages = 0;
  OldSpace faked_space(heap);
  CHECK(!faked_space.first_page());
  CHECK(!faked_space.last_page());
  NormalPage* first_page = memory_allocator->AllocatePage(
      MemoryAllocator::AllocationMode::kRegular, &faked_space, NOT_EXECUTABLE);
  ASSERT_NE(nullptr, first_page);

  faked_space.memory_chunk_list().PushBack(first_page);
  CHECK_NULL(first_page->next_page());
  total_pages++;

  for (NormalPage* p = first_page; p != nullptr; p = p->next_page()) {
    CHECK_EQ(p->owner(), &faked_space);
  }

  // Again, we should get n or n - 1 pages.
  NormalPage* other = memory_allocator->AllocatePage(
      MemoryAllocator::AllocationMode::kRegular, &faked_space, NOT_EXECUTABLE);
  total_pages++;
  faked_space.memory_chunk_list().PushBack(other);
  int page_count = 0;
  for (NormalPage* p = first_page; p != nullptr; p = p->next_page()) {
    CHECK_EQ(p->owner(), &faked_space);
    page_count++;
  }
  CHECK_EQ(total_pages, page_count);

  NormalPage* second_page = first_page->next_page();
  CHECK_NOT_NULL(second_page);

  // OldSpace's destructor will tear down the space and free up all pages.
}

TEST_F(SpacesTest, ComputeDiscardMemoryAreas) {
  std::optional<base::AddressRegion> discard_area;
  size_t page_size = MemoryAllocator::GetCommitPageSize();

  discard_area = Sweeper::ComputeDiscardMemoryArea(0, 0);
  CHECK(!discard_area);

  discard_area = Sweeper::ComputeDiscardMemoryArea(0, page_size);
  CHECK_EQ(discard_area->begin(), 0);
  CHECK_EQ(discard_area->size(), page_size);

  discard_area = Sweeper::ComputeDiscardMemoryArea(page_size, 2 * page_size);
  CHECK_EQ(discard_area->begin(), page_size);
  CHECK_EQ(discard_area->size(), page_size);

  discard_area =
      Sweeper::ComputeDiscardMemoryArea(page_size - kTaggedSize, 2 * page_size);
  CHECK_EQ(discard_area->begin(), page_size);
  CHECK_EQ(discard_area->size(), page_size);

  discard_area =
      Sweeper::ComputeDiscardMemoryArea(page_size, 2 * page_size + kTaggedSize);
  CHECK_EQ(discard_area->begin(), page_size);
  CHECK_EQ(discard_area->size(), page_size);

  discard_area = Sweeper::ComputeDiscardMemoryArea(page_size, page_size);
  CHECK(!discard_area);

  discard_area = Sweeper::ComputeDiscardMemoryArea(page_size / 2,
                                                   page_size + page_size / 2);
  CHECK(!discard_area);

  discard_area = Sweeper::ComputeDiscardMemoryArea(page_size / 2,
                                                   page_size + page_size / 4);
  CHECK(!discard_area);

  discard_area =
      Sweeper::ComputeDiscardMemoryArea(page_size / 2, page_size * 3);
  CHECK_EQ(discard_area->begin(), page_size);
  CHECK_EQ(discard_area->size(), page_size * 2);
}

TEST_F(SpacesTest, SemiSpaceNewSpace) {
  if (v8_flags.single_generation) return;
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  TestMemoryAllocatorScope test_allocator_scope(isolate, heap->MaxReserved());
  MemoryAllocator* memory_allocator = test_allocator_scope.allocator();
  LinearAllocationArea allocation_info;

  auto new_space = std::make_unique<SemiSpaceNewSpace>(
      heap, heap->InitialSemiSpaceSize(), heap->InitialSemiSpaceSize(),
      heap->InitialSemiSpaceSize());
  MainAllocator allocator(heap->main_thread_local_heap(), new_space.get(),
                          MainAllocator::kNewGeneration, &allocation_info);
  CHECK(new_space->MaximumCapacity());

  size_t successful_allocations = 0;
  while (new_space->Available() >= kMaxRegularHeapObjectSize) {
    AllocationResult allocation = allocator.AllocateRaw(
        SafeHeapObjectSize(kMaxRegularHeapObjectSize), kTaggedAligned,
        AllocationOrigin::kRuntime, AllocationHint());
    if (allocation.IsFailure()) break;
    successful_allocations++;
    Tagged<Object> obj = allocation.ToObjectChecked();
    Tagged<HeapObject> ho = Cast<HeapObject>(obj);
    CHECK(new_space->Contains(ho));
  }
  CHECK_LT(0, successful_allocations);

  new_space.reset();
  memory_allocator->ReleasePooledChunksImmediately();
}

TEST_F(SpacesTest, PagedNewSpace) {
  if (v8_flags.single_generation) return;
  ManualGCScope manual_gc_scope(i_isolate());
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  TestMemoryAllocatorScope test_allocator_scope(isolate, heap->MaxReserved());
  MemoryAllocator* memory_allocator = test_allocator_scope.allocator();
  LinearAllocationArea allocation_info;

  auto new_space = std::make_unique<PagedNewSpace>(
      heap, heap->InitialSemiSpaceSize(), heap->InitialSemiSpaceSize(),
      heap->InitialSemiSpaceSize());
  MainAllocator allocator(heap->main_thread_local_heap(), new_space.get(),
                          MainAllocator::kNewGeneration, &allocation_info);
  GrowNewSpaceToMaximumCapacity();

  size_t successful_allocations = 0;
  while (true) {
    AllocationResult allocation = allocator.AllocateRaw(
        SafeHeapObjectSize(kMaxRegularHeapObjectSize), kTaggedAligned,
        AllocationOrigin::kRuntime, AllocationHint());
    if (allocation.IsFailure()) break;
    successful_allocations++;
    Tagged<Object> obj = allocation.ToObjectChecked();
    Tagged<HeapObject> ho = Cast<HeapObject>(obj);
    CHECK(new_space->Contains(ho));
  }
  CHECK_LT(0, successful_allocations);

  new_space.reset();
  memory_allocator->ReleasePooledChunksImmediately();
}

TEST_F(SpacesTestWithSmallHeap, OldSpace) {
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  TestMemoryAllocatorScope test_allocator_scope(isolate, heap->MaxReserved());
  LinearAllocationArea allocation_info;

  auto old_space = std::make_unique<OldSpace>(heap);
  MainAllocator allocator(heap->main_thread_local_heap(), old_space.get(),
                          MainAllocator::kOldGeneration, &allocation_info);
  const int obj_size = kMaxRegularHeapObjectSize;

  size_t successful_allocations = 0;

  while (true) {
    AllocationResult allocation =
        allocator.AllocateRaw(SafeHeapObjectSize(obj_size), kTaggedAligned,
                              AllocationOrigin::kRuntime, AllocationHint());
    if (allocation.IsFailure()) break;
    successful_allocations++;
    Tagged<Object> obj = allocation.ToObjectChecked();
    Tagged<HeapObject> ho = Cast<HeapObject>(obj);
    CHECK(old_space->Contains(ho));
  }
  CHECK_LT(0, successful_allocations);
}

TEST_F(SpacesTestWithSmallHeap, OldLargeObjectSpace) {
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();

  auto lo = std::make_unique<OldLargeObjectSpace>(heap);
  // Handles must not outlive the detached space they point into.
  HandleScope handle_scope(isolate);
  const int lo_size = NormalPage::kPageSize;

  Tagged<Map> map = ReadOnlyRoots(isolate).fixed_double_array_map();
  size_t successful_allocations = 0;

  while (true) {
    AllocationResult allocation = lo->AllocateRaw(
        heap->main_thread_local_heap(), lo_size, AllocationHint());
    if (allocation.IsFailure()) break;
    successful_allocations++;
    Tagged<Object> obj = allocation.ToObjectChecked();
    CHECK(IsHeapObject(obj));
    Tagged<HeapObject> ho = Cast<HeapObject>(obj);
    CHECK(lo->Contains(ho));
    CHECK_EQ(0, MainAllocator::GetFillToAlign(ho.address(), kTaggedAligned));
    // All large objects have the same alignment because they start at the
    // same offset within a page. Fixed double arrays have the most strict
    // alignment requirements.
    CHECK_EQ(0, MainAllocator::GetFillToAlign(
                    ho.address(),
                    HeapObject::RequiredAlignment(lo->identity(), map)));
    DirectHandle<HeapObject> keep_alive(ho, isolate);
  }
  CHECK_LT(0, successful_allocations);

  CHECK(!lo->IsEmpty());
  CHECK(
      lo->AllocateRaw(heap->main_thread_local_heap(), lo_size, AllocationHint())
          .IsFailure());
}

#ifndef DEBUG
// The test verifies that committed size of a space is less then some threshold.
// Debug builds pull in all sorts of additional instrumentation that increases
// heap sizes. E.g. CSA_DCHECK creates on-heap strings for error messages. These
// messages are also not stable if files are moved and modified during the build
// process (jumbo builds).
TEST_F(SpacesTest, SizeOfInitialHeap) {
  Isolate* isolate = i_isolate();
  // Bootstrapping without a snapshot causes more allocations.
  if (!isolate->snapshot_available()) return;
  ManualGCScope manual_gc_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  // Skip this test on the custom snapshot builder.
  if (!context->Global()
           ->Get(context, NewString("assertEquals"))
           .ToLocalChecked()
           ->IsUndefined()) {
    return;
  }
  // Initial size of LO_SPACE
  size_t initial_lo_space = isolate->heap()->lo_space()->Size();

// The limit for each space for an empty isolate containing just the
// snapshot.
// In PPC the page size is 64K, causing more internal fragmentation
// hence requiring a larger limit.
#if V8_OS_LINUX && V8_HOST_ARCH_PPC64
  const size_t kMaxInitialSizePerSpace = 3 * MB;
#else
  const size_t kMaxInitialSizePerSpace = 2 * MB;
#endif

  // Freshly initialized VM gets by with the snapshot size (which is below
  // kMaxInitialSizePerSpace per space).
  Heap* heap = isolate->heap();
  for (int i = FIRST_GROWABLE_PAGED_SPACE; i <= LAST_GROWABLE_PAGED_SPACE;
       i++) {
    if (!heap->paged_space(i)) continue;

    // Debug code can be very large, so skip CODE_SPACE if we are generating it.
    if (i == CODE_SPACE && i::v8_flags.debug_code) continue;

    // Check that the initial heap is also below the limit.
    CHECK_LE(heap->paged_space(i)->CommittedMemory(), kMaxInitialSizePerSpace);
  }

  RunJS("/*empty*/");

  // No large objects required to perform the above steps.
  CHECK_EQ(initial_lo_space,
           static_cast<size_t>(isolate->heap()->lo_space()->Size()));
}
#endif  // DEBUG

TEST_F(SpacesTest, Regress777177) {
  SaveFlags save_flags;
  v8_flags.stress_concurrent_allocation = false;  // For SimulateFullSpace.
  v8::Context::Scope context_scope(v8::Context::New(v8_isolate()));
  Heap* heap = i_isolate()->heap();
  OldSpace* old_space = heap->old_space();
  MainAllocator* old_space_allocator = heap->allocator()->old_space_allocator();
  Observer observer(128);
  old_space_allocator->FreeLinearAllocationArea();
  old_space_allocator->AddAllocationObserver(&observer);

  int area_size = old_space->AreaSize();
  int max_object_size = kMaxRegularHeapObjectSize;
  int filler_size = area_size - max_object_size;

  {
    // Ensure a new linear allocation area on a fresh page.
    AlwaysAllocateScopeForTesting always_allocate(heap);
    SimulateFullSpace(old_space);
    AllocationResult result = old_space_allocator->AllocateRaw(
        SafeHeapObjectSize(filler_size), kTaggedAligned,
        AllocationOrigin::kRuntime, AllocationHint());
    Tagged<HeapObject> obj = result.ToObjectChecked();
    heap->CreateFillerObjectAt(obj.address(), filler_size);
  }

  {
    // Allocate all bytes of the linear allocation area. This moves top_ and
    // top_on_previous_step_ to the next page.
    AllocationResult result = old_space_allocator->AllocateRaw(
        SafeHeapObjectSize(max_object_size), kTaggedAligned,
        AllocationOrigin::kRuntime, AllocationHint());
    Tagged<HeapObject> obj = result.ToObjectChecked();
    // Simulate allocation folding moving the top pointer back.
    old_space_allocator->ResetLab(
        obj.address(), heap->allocator()->old_space_allocator()->limit(),
        heap->allocator()->old_space_allocator()->limit());
  }

  {
    // This triggers assert in crbug.com/777177.
    AllocationResult result = old_space_allocator->AllocateRaw(
        SafeHeapObjectSize(filler_size), kTaggedAligned,
        AllocationOrigin::kRuntime, AllocationHint());
    Tagged<HeapObject> obj = result.ToObjectChecked();
    heap->CreateFillerObjectAt(obj.address(), filler_size);
  }
  old_space_allocator->RemoveAllocationObserver(&observer);
}

TEST_F(SpacesTest, Regress791582) {
  if (v8_flags.single_generation) return;
  v8::Context::Scope context_scope(v8::Context::New(v8_isolate()));
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  MainAllocator* new_space_allocator = heap->allocator()->new_space_allocator();
  GrowNewSpaceToMaximumCapacity();

  int until_page_end =
      static_cast<int>(heap->NewSpaceLimit() - heap->NewSpaceTop());

  if (!IsAligned(until_page_end, kTaggedSize)) {
    // The test works if the size of allocation area size is a multiple of
    // pointer size. This is usually the case unless some allocation observer
    // is already active (e.g. incremental marking observer).
    return;
  }

  Observer observer(128);
  new_space_allocator->FreeLinearAllocationArea();
  new_space_allocator->AddAllocationObserver(&observer);

  {
    AllocationResult result = new_space_allocator->AllocateRaw(
        SafeHeapObjectSize(until_page_end), kTaggedAligned,
        AllocationOrigin::kRuntime, AllocationHint());
    Tagged<HeapObject> obj = result.ToObjectChecked();
    heap->CreateFillerObjectAt(obj.address(), until_page_end);
    // Simulate allocation folding moving the top pointer back.
    LinearAllocationArea* new_space =
        &isolate->isolate_data()->new_allocation_info();
    *new_space->top_address() = obj.address();
  }

  {
    // This triggers assert in crbug.com/791582
    AllocationResult result = new_space_allocator->AllocateRaw(
        SafeHeapObjectSize(256), kTaggedAligned, AllocationOrigin::kRuntime,
        AllocationHint());
    Tagged<HeapObject> obj = result.ToObjectChecked();
    heap->CreateFillerObjectAt(obj.address(), 256);
  }
  new_space_allocator->RemoveAllocationObserver(&observer);
}

TEST_F(SpacesTest, NoMemoryForNewPage) {
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();

  // Memory allocator that will fail to allocate any pages.
  FailingPageAllocator failing_allocator;
  TestMemoryAllocatorScope test_allocator_scope(isolate, 0, &failing_allocator);
  MemoryAllocator* memory_allocator = test_allocator_scope.allocator();
  OldSpace faked_space(heap);
  NormalPage* page = memory_allocator->AllocatePage(
      MemoryAllocator::AllocationMode::kRegular, &faked_space, NOT_EXECUTABLE);

  CHECK_NULL(page);
}

TEST_F(SpacesTest, ReadOnlySpaceMetrics_OnePage) {
  // Create a read-only space and allocate some memory, shrink the pages and
  // check the allocated object size is as expected.

  ReadOnlySpaceScope scope(heap());
  ReadOnlySpace* faked_space = scope.space();

  // Initially no memory.
  CHECK_EQ(faked_space->Size(), 0);
  CHECK_EQ(faked_space->Capacity(), 0);
  CHECK_EQ(faked_space->CommittedMemory(), 0);
  CHECK_EQ(faked_space->CommittedPhysicalMemory(), 0);

  faked_space->AllocateRaw(16, kTaggedAligned);

  faked_space->ShrinkPages();
  faked_space->Seal(ReadOnlySpace::SealMode::kDoNotDetachFromHeap);

  // Allocated objects size.
  CHECK_EQ(faked_space->Size(), 16);

  size_t committed_memory = RoundUp(
      MemoryChunkLayout::ObjectStartOffsetInDataPage() + faked_space->Size(),
      MemoryAllocator::GetCommitPageSize());

  // Amount of OS allocated memory.
  CHECK_EQ(faked_space->CommittedMemory(), committed_memory);
  CHECK_EQ(faked_space->CommittedPhysicalMemory(), committed_memory);

  // Capacity will be one OS page minus the page header.
  CHECK_EQ(faked_space->Capacity(),
           committed_memory - MemoryChunkLayout::ObjectStartOffsetInDataPage());
}

TEST_F(SpacesTest, ReadOnlySpaceMetrics_AlignedAllocations) {
  // Create a read-only space and allocate some memory, shrink the pages and
  // check the allocated object size is as expected.

  ReadOnlySpaceScope scope(heap());
  ReadOnlySpace* faked_space = scope.space();

  // Initially no memory.
  CHECK_EQ(faked_space->Size(), 0);
  CHECK_EQ(faked_space->Capacity(), 0);
  CHECK_EQ(faked_space->CommittedMemory(), 0);
  CHECK_EQ(faked_space->CommittedPhysicalMemory(), 0);

  // Allocate an object just under an OS page in size.
  int object_size =
      static_cast<int>(MemoryAllocator::GetCommitPageSize() - kApiTaggedSize);

  const int kExpectedAlignment = kDoubleSize;

  Tagged<HeapObject> object =
      faked_space->AllocateRaw(object_size, kDoubleAligned).ToObjectChecked();
  CHECK_EQ(object.address() % kExpectedAlignment, 0);
  object =
      faked_space->AllocateRaw(object_size, kDoubleAligned).ToObjectChecked();
  CHECK_EQ(object.address() % kExpectedAlignment, 0);

  // Calculate size of allocations based on area_start.
  Address area_start = faked_space->pages().back()->GetAreaStart();
  Address top = RoundUp(area_start, kExpectedAlignment) + object_size;
  top = RoundUp(top, kExpectedAlignment) + object_size;
  size_t expected_size = top - area_start;

  faked_space->ShrinkPages();
  faked_space->Seal(ReadOnlySpace::SealMode::kDoNotDetachFromHeap);

  // Allocated objects size may will contain 4 bytes of padding on 32-bit or
  // with pointer compression.
  CHECK_EQ(faked_space->Size(), expected_size);

  size_t committed_memory = RoundUp(
      MemoryChunkLayout::ObjectStartOffsetInDataPage() + faked_space->Size(),
      MemoryAllocator::GetCommitPageSize());

  CHECK_EQ(faked_space->CommittedMemory(), committed_memory);
  CHECK_EQ(faked_space->CommittedPhysicalMemory(), committed_memory);

  // Capacity will be 3 OS pages minus the page header.
  CHECK_EQ(faked_space->Capacity(),
           committed_memory - MemoryChunkLayout::ObjectStartOffsetInDataPage());
}

TEST_F(SpacesTest, ReadOnlySpaceMetrics_TwoPages) {
  // Create a read-only space and allocate some memory, shrink the pages and
  // check the allocated object size is as expected.

  ReadOnlySpaceScope scope(heap());
  ReadOnlySpace* faked_space = scope.space();

  // Initially no memory.
  CHECK_EQ(faked_space->Size(), 0);
  CHECK_EQ(faked_space->Capacity(), 0);
  CHECK_EQ(faked_space->CommittedMemory(), 0);
  CHECK_EQ(faked_space->CommittedPhysicalMemory(), 0);

  // Allocate an object that's too big to have more than one on a page.

  int object_size = RoundUp(
      static_cast<int>(
          MemoryChunkLayout::AllocatableMemoryInMemoryChunk(RO_SPACE) / 2 + 16),
      kTaggedSize);
  CHECK_GT(object_size * 2,
           MemoryChunkLayout::AllocatableMemoryInMemoryChunk(RO_SPACE));
  faked_space->AllocateRaw(object_size, kTaggedAligned);

  // Then allocate another so it expands the space to two pages.
  faked_space->AllocateRaw(object_size, kTaggedAligned);

  faked_space->ShrinkPages();
  faked_space->Seal(ReadOnlySpace::SealMode::kDoNotDetachFromHeap);

  // Allocated objects size.
  CHECK_EQ(faked_space->Size(), object_size * 2);

  // Amount of OS allocated memory.
  size_t committed_memory_per_page =
      RoundUp(MemoryChunkLayout::ObjectStartOffsetInDataPage() + object_size,
              MemoryAllocator::GetCommitPageSize());
  CHECK_EQ(faked_space->CommittedMemory(), 2 * committed_memory_per_page);
  CHECK_EQ(faked_space->CommittedPhysicalMemory(),
           2 * committed_memory_per_page);

  // Capacity will be the space up to the amount of committed memory minus the
  // page headers.
  size_t capacity_per_page =
      RoundUp(MemoryChunkLayout::ObjectStartOffsetInDataPage() + object_size,
              MemoryAllocator::GetCommitPageSize()) -
      MemoryChunkLayout::ObjectStartOffsetInDataPage();
  CHECK_EQ(faked_space->Capacity(), 2 * capacity_per_page);
}

// This test is currently incompatible with the sandbox. Enable it
// once the VirtualAddressSpace interface is stable.
#if !V8_OS_FUCHSIA && !V8_ENABLE_SANDBOX

namespace {

// This is a v8::PageAllocator implementation that decorates provided page
// allocator object with page tracking functionality.
class TrackingPageAllocator : public ::v8::PageAllocator {
 public:
  explicit TrackingPageAllocator(v8::PageAllocator* page_allocator)
      : page_allocator_(page_allocator),
        allocate_page_size_(page_allocator_->AllocatePageSize()),
        commit_page_size_(page_allocator_->CommitPageSize()),
        region_allocator_(kNullAddress, size_t{0} - commit_page_size_,
                          commit_page_size_) {
    CHECK_NOT_NULL(page_allocator);
    CHECK(IsAligned(allocate_page_size_, commit_page_size_));
  }
  ~TrackingPageAllocator() override = default;

  size_t AllocatePageSize() override { return allocate_page_size_; }

  size_t CommitPageSize() override { return commit_page_size_; }

  void SetRandomMmapSeed(int64_t seed) override {
    return page_allocator_->SetRandomMmapSeed(seed);
  }

  void* GetRandomMmapAddr() override {
    return page_allocator_->GetRandomMmapAddr();
  }

  void* AllocatePages(void* address, size_t size, size_t alignment,
                      PageAllocator::Permission access) override {
    void* result =
        page_allocator_->AllocatePages(address, size, alignment, access);
    if (result) {
      // Mark pages as used.
      Address current_page = reinterpret_cast<Address>(result);
      CHECK(IsAligned(current_page, allocate_page_size_));
      CHECK(IsAligned(size, allocate_page_size_));
      CHECK(region_allocator_.AllocateRegionAt(current_page, size));
      Address end = current_page + size;
      while (current_page < end) {
        PageState state{access, access != kNoAccess};
        page_permissions_.insert({current_page, state});
        current_page += commit_page_size_;
      }
    }
    return result;
  }

  bool FreePages(void* address, size_t size) override {
    bool result = page_allocator_->FreePages(address, size);
    if (result) {
      // Mark pages as free.
      Address start = reinterpret_cast<Address>(address);
      CHECK(IsAligned(start, allocate_page_size_));
      CHECK(IsAligned(size, allocate_page_size_));
      size_t freed_size = region_allocator_.FreeRegion(start);
      CHECK(IsAligned(freed_size, commit_page_size_));
      CHECK_EQ(RoundUp(freed_size, allocate_page_size_), size);
      auto start_iter = page_permissions_.find(start);
      CHECK_NE(start_iter, page_permissions_.end());
      auto end_iter = page_permissions_.lower_bound(start + size);
      page_permissions_.erase(start_iter, end_iter);
    }
    return result;
  }

  bool ReleasePages(void* address, size_t size, size_t new_size) override {
    bool result = page_allocator_->ReleasePages(address, size, new_size);
    if (result) {
      Address start = reinterpret_cast<Address>(address);
      CHECK(IsAligned(start, allocate_page_size_));
      CHECK(IsAligned(size, commit_page_size_));
      CHECK(IsAligned(new_size, commit_page_size_));
      CHECK_LT(new_size, size);
      CHECK_EQ(region_allocator_.TrimRegion(start, new_size), size - new_size);
      auto start_iter = page_permissions_.find(start + new_size);
      CHECK_NE(start_iter, page_permissions_.end());
      auto end_iter = page_permissions_.lower_bound(start + size);
      page_permissions_.erase(start_iter, end_iter);
    }
    return result;
  }

  bool RecommitPages(void* address, size_t size,
                     PageAllocator::Permission access) override {
    bool result = page_allocator_->RecommitPages(address, size, access);
    if (result) {
      // Check that given range had given access permissions.
      CheckPagePermissions(reinterpret_cast<Address>(address), size, access,
                           {});
      UpdatePagePermissions(reinterpret_cast<Address>(address), size, access,
                            true);
    }
    return result;
  }

  bool DiscardSystemPages(void* address, size_t size) override {
    bool result = page_allocator_->DiscardSystemPages(address, size);
    if (result) {
      UpdatePagePermissions(reinterpret_cast<Address>(address), size, {},
                            false);
    }
    return result;
  }

  bool DecommitPages(void* address, size_t size) override {
    bool result = page_allocator_->DecommitPages(address, size);
    if (result) {
      // Mark pages as non-accessible.
      UpdatePagePermissions(reinterpret_cast<Address>(address), size, kNoAccess,
                            false);
    }
    return result;
  }

  bool SetPermissions(void* address, size_t size,
                      PageAllocator::Permission access) override {
    bool result = page_allocator_->SetPermissions(address, size, access);
    if (result) {
      bool committed = access != kNoAccess && access != kNoAccessWillJitLater;
      UpdatePagePermissions(reinterpret_cast<Address>(address), size, access,
                            committed);
    }
    return result;
  }

  // Returns true if all the allocated pages were freed.
  bool IsEmpty() { return page_permissions_.empty(); }

  void CheckIsFree(Address address, size_t size) {
    CHECK(IsAligned(address, allocate_page_size_));
    CHECK(IsAligned(size, allocate_page_size_));
    EXPECT_TRUE(region_allocator_.IsFree(address, size));
  }

  void CheckPagePermissions(Address address, size_t size,
                            PageAllocator::Permission access,
                            std::optional<bool> committed = {true}) {
    CHECK_IMPLIES(committed.has_value() && committed.value(),
                  access != PageAllocator::kNoAccess);
    ForEachPage(address, size, [=](PagePermissionsMap::value_type* value) {
      if (committed.has_value()) {
        EXPECT_EQ(committed.value(), value->second.committed);
      }
      EXPECT_EQ(access, value->second.access);
    });
  }

  void Print(const char* comment) const {
    i::StdoutStream os;
    os << "\n========================================="
       << "\nTracingPageAllocator state: ";
    if (comment) os << comment;
    os << "\n-----------------------------------------\n";
    region_allocator_.Print(os);
    os << "-----------------------------------------"
       << "\nPage permissions:";
    if (page_permissions_.empty()) {
      os << " empty\n";
      return;
    }
    os << "\n" << std::hex << std::showbase;

    Address contiguous_region_start = static_cast<Address>(-1);
    Address contiguous_region_end = contiguous_region_start;
    PageAllocator::Permission contiguous_region_access =
        PageAllocator::kNoAccess;
    bool contiguous_region_access_committed = false;
    for (auto& pair : page_permissions_) {
      if (contiguous_region_end == pair.first &&
          pair.second.access == contiguous_region_access &&
          pair.second.committed == contiguous_region_access_committed) {
        contiguous_region_end += commit_page_size_;
        continue;
      }
      if (contiguous_region_start != contiguous_region_end) {
        PrintRegion(os, contiguous_region_start, contiguous_region_end,
                    contiguous_region_access,
                    contiguous_region_access_committed);
      }
      contiguous_region_start = pair.first;
      contiguous_region_end = pair.first + commit_page_size_;
      contiguous_region_access = pair.second.access;
      contiguous_region_access_committed = pair.second.committed;
    }
    if (contiguous_region_start != contiguous_region_end) {
      PrintRegion(os, contiguous_region_start, contiguous_region_end,
                  contiguous_region_access, contiguous_region_access_committed);
    }
  }

 private:
  struct PageState {
    PageAllocator::Permission access;
    bool committed;
  };
  using PagePermissionsMap = std::map<Address, PageState>;
  using ForEachFn = std::function<void(PagePermissionsMap::value_type*)>;

  static void PrintRegion(std::ostream& os, Address start, Address end,
                          PageAllocator::Permission access, bool committed) {
    os << "  page: [" << start << ", " << end << "), access: ";
    switch (access) {
      case PageAllocator::kNoAccess:
      case PageAllocator::kNoAccessWillJitLater:
        os << "--";
        break;
      case PageAllocator::kRead:
        os << "R";
        break;
      case PageAllocator::kReadWrite:
        os << "RW";
        break;
      case PageAllocator::kReadWriteExecute:
        os << "RWX";
        break;
      case PageAllocator::kReadExecute:
        os << "RX";
        break;
    }
    os << ", committed: " << static_cast<int>(committed) << "\n";
  }

  void ForEachPage(Address address, size_t size, const ForEachFn& fn) {
    CHECK(IsAligned(address, commit_page_size_));
    CHECK(IsAligned(size, commit_page_size_));
    auto start_iter = page_permissions_.find(address);
    // Start page must exist in page_permissions_.
    CHECK_NE(start_iter, page_permissions_.end());
    auto end_iter = page_permissions_.find(address + size - commit_page_size_);
    // Ensure the last but one page exists in page_permissions_.
    CHECK_NE(end_iter, page_permissions_.end());
    // Now make it point to the next element in order to also process is by the
    // following for loop.
    ++end_iter;
    for (auto iter = start_iter; iter != end_iter; ++iter) {
      PagePermissionsMap::value_type& pair = *iter;
      fn(&pair);
    }
  }

  void UpdatePagePermissions(Address address, size_t size,
                             std::optional<PageAllocator::Permission> access,
                             bool committed) {
    ForEachPage(address, size, [=](PagePermissionsMap::value_type* value) {
      if (access.has_value()) {
        value->second.access = access.value();
      }
      value->second.committed = committed;
    });
  }

  v8::PageAllocator* const page_allocator_;
  const size_t allocate_page_size_;
  const size_t commit_page_size_;
  // Region allocator tracks page allocation/deallocation requests.
  base::RegionAllocator region_allocator_;
  // This map keeps track of allocated pages' permissions.
  PagePermissionsMap page_permissions_;
};

template <typename TMixin>
class PoolTestMixin : public TMixin {
 public:
  PoolTestMixin();
  ~PoolTestMixin() override;
};

}  // namespace

// PoolTest must live in v8::internal because IsolateGroup befriends it under
// that name (see isolate-group.h).
class PoolTest : public                                     //
                 WithInternalIsolateMixin<                  //
                     WithIsolateScopeMixin<                 //
                         WithIsolateMixin<                  //
                             PoolTestMixin<                 //
                                 WithDefaultPlatformMixin<  //
                                     ::testing::Test>>>>> {
 public:
  PoolTest() = default;
  ~PoolTest() override = default;
  PoolTest(const PoolTest&) = delete;
  PoolTest& operator=(const PoolTest&) = delete;

  static void DoMixinSetUp() {
    CHECK_NULL(tracking_page_allocator_);
    old_page_allocator_ = GetPlatformPageAllocator();
    tracking_page_allocator_ = new TrackingPageAllocator(old_page_allocator_);
    CHECK(tracking_page_allocator_->IsEmpty());
    CHECK_EQ(old_page_allocator_,
             SetPlatformPageAllocatorForTesting(tracking_page_allocator_));
    old_sweeping_flag_ = i::v8_flags.concurrent_sweeping;
    i::v8_flags.concurrent_sweeping = false;
    IsolateGroup::ReleaseDefault();
    IsolateGroup::InitializeOncePerProcess();
  }

  static void DoMixinTearDown() {
    IsolateGroup::ReleaseDefault();
    i::v8_flags.concurrent_sweeping = old_sweeping_flag_;
    CHECK(tracking_page_allocator_->IsEmpty());

    // Restore the original v8::PageAllocator and delete the tracking one.
    CHECK_EQ(tracking_page_allocator_,
             SetPlatformPageAllocatorForTesting(old_page_allocator_));
    delete tracking_page_allocator_;
    tracking_page_allocator_ = nullptr;

    IsolateGroup::InitializeOncePerProcess();
  }

  Heap* heap() { return isolate()->heap(); }
  MemoryAllocator* allocator() { return heap()->memory_allocator(); }
  MemoryPool* pool() { return isolate()->isolate_group()->memory_pool(); }

  TrackingPageAllocator* tracking_page_allocator() {
    return tracking_page_allocator_;
  }

 private:
  static TrackingPageAllocator* tracking_page_allocator_;
  static v8::PageAllocator* old_page_allocator_;
  static bool old_sweeping_flag_;
};

TrackingPageAllocator* PoolTest::tracking_page_allocator_ = nullptr;
v8::PageAllocator* PoolTest::old_page_allocator_ = nullptr;
bool PoolTest::old_sweeping_flag_;

template <typename TMixin>
PoolTestMixin<TMixin>::PoolTestMixin() {
  PoolTest::DoMixinSetUp();
}
template <typename TMixin>
PoolTestMixin<TMixin>::~PoolTestMixin() {
  PoolTest::DoMixinTearDown();
}

// See v8:5945.
TEST_F(PoolTest, UnmapOnTeardown) {
  // Wait for the task to finish and disable rescheduling.
  pool()->CancelAndWaitForTaskToFinishForTesting();

  NormalPage* page =
      allocator()->AllocatePage(MemoryAllocator::AllocationMode::kRegular,
                                static_cast<PagedSpace*>(heap()->old_space()),
                                Executability::NOT_EXECUTABLE);
  Address chunk_address = page->ChunkAddress();
  EXPECT_NE(nullptr, page);
  const size_t page_size = tracking_page_allocator()->AllocatePageSize();
  tracking_page_allocator()->CheckPagePermissions(chunk_address, page_size,
                                                  PageAllocator::kReadWrite);

  allocator()->Free(MemoryAllocator::FreeMode::kPool, page);
  tracking_page_allocator()->CheckPagePermissions(chunk_address, page_size,
                                                  PageAllocator::kReadWrite);
  pool()->ReleaseImmediately(i_isolate());
#ifdef V8_COMPRESS_POINTERS
  // In this mode Isolate uses bounded page allocator which allocates pages
  // inside prereserved region. Thus these pages are kept reserved until
  // the Isolate dies.
  tracking_page_allocator()->CheckPagePermissions(
      chunk_address, page_size, PageAllocator::kNoAccess, false);
#else
  tracking_page_allocator()->CheckIsFree(chunk_address, page_size);
#endif  // V8_COMPRESS_POINTERS

  // Wait for the task to finish and disable rescheduling.
  pool()->ReenableTaskForTesting();
}
#endif  // !V8_OS_FUCHSIA && !V8_ENABLE_SANDBOX

TEST_F(SpacesTest, EagerDiscardingInCollectAllAvailableGarbage) {
  // --stress-concurrent-allocation is re-checked by the heap's allocation
  // observer on every step, so disabling it on the already created isolate is
  // sufficient for SimulateFullSpace.
  FLAG_VALUE_SCOPE(stress_concurrent_allocation, false);
  SimulateFullSpace(heap()->old_space());
  InvokeMemoryReducingMajorGCs();
  // The pooled chunk count is IsolateGroup-wide, so this relies on the fixture
  // isolate being the only isolate in the group.
  EXPECT_EQ(0u, heap()->memory_allocator()->GetPooledChunksCount());
}

}  // namespace internal
}  // namespace v8
