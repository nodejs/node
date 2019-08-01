// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "src/base/bounded-page-allocator.h"
#include "src/base/platform/platform.h"
#include "src/heap/factory.h"
#include "src/heap/spaces-inl.h"
#include "src/objects/free-space.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/snapshot.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

// Temporarily sets a given allocator in an isolate.
class TestMemoryAllocatorScope {
 public:
  TestMemoryAllocatorScope(Isolate* isolate, size_t max_capacity,
                           size_t code_range_size)
      : isolate_(isolate),
        old_allocator_(std::move(isolate->heap()->memory_allocator_)) {
    isolate->heap()->memory_allocator_.reset(
        new MemoryAllocator(isolate, max_capacity, code_range_size));
  }

  MemoryAllocator* allocator() { return isolate_->heap()->memory_allocator(); }

  ~TestMemoryAllocatorScope() {
    isolate_->heap()->memory_allocator()->TearDown();
    isolate_->heap()->memory_allocator_.swap(old_allocator_);
  }

 private:
  Isolate* isolate_;
  std::unique_ptr<MemoryAllocator> old_allocator_;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryAllocatorScope);
};

// Temporarily sets a given code page allocator in an isolate.
class TestCodePageAllocatorScope {
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

 private:
  Isolate* isolate_;
  v8::PageAllocator* old_code_page_allocator_;

  DISALLOW_COPY_AND_ASSIGN(TestCodePageAllocatorScope);
};

static void VerifyMemoryChunk(Isolate* isolate, Heap* heap,
                              v8::PageAllocator* code_page_allocator,
                              size_t reserve_area_size, size_t commit_area_size,
                              Executability executable, Space* space) {
  TestMemoryAllocatorScope test_allocator_scope(isolate, heap->MaxReserved(),
                                                0);
  MemoryAllocator* memory_allocator = test_allocator_scope.allocator();
  TestCodePageAllocatorScope test_code_page_allocator_scope(
      isolate, code_page_allocator);

  v8::PageAllocator* page_allocator =
      memory_allocator->page_allocator(executable);

  size_t allocatable_memory_area_offset =
      MemoryChunkLayout::ObjectStartOffsetInMemoryChunk(space->identity());
  size_t guard_size =
      (executable == EXECUTABLE) ? MemoryChunkLayout::CodePageGuardSize() : 0;

  MemoryChunk* memory_chunk = memory_allocator->AllocateChunk(
      reserve_area_size, commit_area_size, executable, space);
  size_t reserved_size =
      ((executable == EXECUTABLE))
          ? allocatable_memory_area_offset +
                RoundUp(reserve_area_size, page_allocator->CommitPageSize()) +
                guard_size
          : RoundUp(allocatable_memory_area_offset + reserve_area_size,
                    page_allocator->CommitPageSize());
  CHECK(memory_chunk->size() == reserved_size);
  CHECK(memory_chunk->area_start() <
        memory_chunk->address() + memory_chunk->size());
  CHECK(memory_chunk->area_end() <=
        memory_chunk->address() + memory_chunk->size());
  CHECK(static_cast<size_t>(memory_chunk->area_size()) == commit_area_size);

  memory_allocator->Free<MemoryAllocator::kFull>(memory_chunk);
}

static unsigned int PseudorandomAreaSize() {
  static uint32_t lo = 2345;
  lo = 18273 * (lo & 0xFFFFF) + (lo >> 16);
  return lo & 0xFFFFF;
}


TEST(MemoryChunk) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();

  size_t reserve_area_size = 1 * MB;
  size_t initial_commit_area_size;

  for (int i = 0; i < 100; i++) {
    initial_commit_area_size =
        RoundUp(PseudorandomAreaSize(), page_allocator->CommitPageSize());

    // With CodeRange.
    const size_t code_range_size = 32 * MB;
    VirtualMemory code_range_reservation(page_allocator, code_range_size,
                                         nullptr, MemoryChunk::kAlignment);
    CHECK(code_range_reservation.IsReserved());

    base::BoundedPageAllocator code_page_allocator(
        page_allocator, code_range_reservation.address(),
        code_range_reservation.size(), MemoryChunk::kAlignment);

    VerifyMemoryChunk(isolate, heap, &code_page_allocator, reserve_area_size,
                      initial_commit_area_size, EXECUTABLE, heap->code_space());

    VerifyMemoryChunk(isolate, heap, &code_page_allocator, reserve_area_size,
                      initial_commit_area_size, NOT_EXECUTABLE,
                      heap->old_space());
  }
}


TEST(MemoryAllocator) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  TestMemoryAllocatorScope test_allocator_scope(isolate, heap->MaxReserved(),
                                                0);
  MemoryAllocator* memory_allocator = test_allocator_scope.allocator();

  int total_pages = 0;
  OldSpace faked_space(heap);
  CHECK(!faked_space.first_page());
  CHECK(!faked_space.last_page());
  Page* first_page = memory_allocator->AllocatePage(
      faked_space.AreaSize(), static_cast<PagedSpace*>(&faked_space),
      NOT_EXECUTABLE);

  faked_space.memory_chunk_list().PushBack(first_page);
  CHECK(first_page->next_page() == nullptr);
  total_pages++;

  for (Page* p = first_page; p != nullptr; p = p->next_page()) {
    CHECK(p->owner() == &faked_space);
  }

  // Again, we should get n or n - 1 pages.
  Page* other = memory_allocator->AllocatePage(
      faked_space.AreaSize(), static_cast<PagedSpace*>(&faked_space),
      NOT_EXECUTABLE);
  total_pages++;
  faked_space.memory_chunk_list().PushBack(other);
  int page_count = 0;
  for (Page* p = first_page; p != nullptr; p = p->next_page()) {
    CHECK(p->owner() == &faked_space);
    page_count++;
  }
  CHECK(total_pages == page_count);

  Page* second_page = first_page->next_page();
  CHECK_NOT_NULL(second_page);

  // OldSpace's destructor will tear down the space and free up all pages.
}

TEST(ComputeDiscardMemoryAreas) {
  base::AddressRegion memory_area;
  size_t page_size = MemoryAllocator::GetCommitPageSize();
  size_t free_header_size = FreeSpace::kSize;

  memory_area = MemoryAllocator::ComputeDiscardMemoryArea(0, 0);
  CHECK_EQ(memory_area.begin(), 0);
  CHECK_EQ(memory_area.size(), 0);

  memory_area = MemoryAllocator::ComputeDiscardMemoryArea(
      0, page_size + free_header_size);
  CHECK_EQ(memory_area.begin(), 0);
  CHECK_EQ(memory_area.size(), 0);

  memory_area = MemoryAllocator::ComputeDiscardMemoryArea(
      page_size - free_header_size, page_size + free_header_size);
  CHECK_EQ(memory_area.begin(), page_size);
  CHECK_EQ(memory_area.size(), page_size);

  memory_area = MemoryAllocator::ComputeDiscardMemoryArea(page_size, page_size);
  CHECK_EQ(memory_area.begin(), 0);
  CHECK_EQ(memory_area.size(), 0);

  memory_area = MemoryAllocator::ComputeDiscardMemoryArea(
      page_size / 2, page_size + page_size / 2);
  CHECK_EQ(memory_area.begin(), page_size);
  CHECK_EQ(memory_area.size(), page_size);

  memory_area = MemoryAllocator::ComputeDiscardMemoryArea(
      page_size / 2, page_size + page_size / 4);
  CHECK_EQ(memory_area.begin(), 0);
  CHECK_EQ(memory_area.size(), 0);

  memory_area =
      MemoryAllocator::ComputeDiscardMemoryArea(page_size / 2, page_size * 3);
  CHECK_EQ(memory_area.begin(), page_size);
  CHECK_EQ(memory_area.size(), page_size * 2);
}

TEST(NewSpace) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  TestMemoryAllocatorScope test_allocator_scope(isolate, heap->MaxReserved(),
                                                0);
  MemoryAllocator* memory_allocator = test_allocator_scope.allocator();

  NewSpace new_space(heap, memory_allocator->data_page_allocator(),
                     CcTest::heap()->InitialSemiSpaceSize(),
                     CcTest::heap()->InitialSemiSpaceSize());
  CHECK(new_space.MaximumCapacity());

  while (new_space.Available() >= kMaxRegularHeapObjectSize) {
    CHECK(new_space.Contains(
        new_space.AllocateRawUnaligned(kMaxRegularHeapObjectSize)
            .ToObjectChecked()));
  }

  new_space.TearDown();
  memory_allocator->unmapper()->EnsureUnmappingCompleted();
}


TEST(OldSpace) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  TestMemoryAllocatorScope test_allocator_scope(isolate, heap->MaxReserved(),
                                                0);

  OldSpace* s = new OldSpace(heap);
  CHECK_NOT_NULL(s);

  while (s->Available() > 0) {
    s->AllocateRawUnaligned(kMaxRegularHeapObjectSize).ToObjectChecked();
  }

  delete s;
}

TEST(LargeObjectSpace) {
  // This test does not initialize allocated objects, which confuses the
  // incremental marker.
  FLAG_incremental_marking = false;
  v8::V8::Initialize();

  LargeObjectSpace* lo = CcTest::heap()->lo_space();
  CHECK_NOT_NULL(lo);

  int lo_size = Page::kPageSize;

  Object obj = lo->AllocateRaw(lo_size).ToObjectChecked();
  CHECK(obj.IsHeapObject());

  HeapObject ho = HeapObject::cast(obj);

  CHECK(lo->Contains(HeapObject::cast(obj)));

  CHECK(lo->Contains(ho));

  while (true) {
    {
      AllocationResult allocation = lo->AllocateRaw(lo_size);
      if (allocation.IsRetry()) break;
    }
  }

  CHECK(!lo->IsEmpty());

  CHECK(lo->AllocateRaw(lo_size).IsRetry());
}

#ifndef DEBUG
// The test verifies that committed size of a space is less then some threshold.
// Debug builds pull in all sorts of additional instrumentation that increases
// heap sizes. E.g. CSA_ASSERT creates on-heap strings for error messages. These
// messages are also not stable if files are moved and modified during the build
// process (jumbo builds).
TEST(SizeOfInitialHeap) {
  if (i::FLAG_always_opt) return;
  // Bootstrapping without a snapshot causes more allocations.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  if (!isolate->snapshot_available()) return;
  HandleScope scope(isolate);
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  // Skip this test on the custom snapshot builder.
  if (!CcTest::global()
           ->Get(context, v8_str("assertEquals"))
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
#if V8_OS_LINUX && V8_HOST_ARCH_PPC
  const size_t kMaxInitialSizePerSpace = 3 * MB;
#else
  const size_t kMaxInitialSizePerSpace = 2 * MB;
#endif

  // Freshly initialized VM gets by with the snapshot size (which is below
  // kMaxInitialSizePerSpace per space).
  Heap* heap = isolate->heap();
  int page_count[LAST_GROWABLE_PAGED_SPACE + 1] = {0, 0, 0, 0};
  for (int i = FIRST_GROWABLE_PAGED_SPACE; i <= LAST_GROWABLE_PAGED_SPACE;
       i++) {
    // Debug code can be very large, so skip CODE_SPACE if we are generating it.
    if (i == CODE_SPACE && i::FLAG_debug_code) continue;

    page_count[i] = heap->paged_space(i)->CountTotalPages();
    // Check that the initial heap is also below the limit.
    CHECK_LE(heap->paged_space(i)->CommittedMemory(), kMaxInitialSizePerSpace);
  }

  // Executing the empty script gets by with the same number of pages, i.e.,
  // requires no extra space.
  CompileRun("/*empty*/");
  for (int i = FIRST_GROWABLE_PAGED_SPACE; i <= LAST_GROWABLE_PAGED_SPACE;
       i++) {
    // Skip CODE_SPACE, since we had to generate code even for an empty script.
    if (i == CODE_SPACE) continue;
    CHECK_EQ(page_count[i], isolate->heap()->paged_space(i)->CountTotalPages());
  }

  // No large objects required to perform the above steps.
  CHECK_EQ(initial_lo_space,
           static_cast<size_t>(isolate->heap()->lo_space()->Size()));
}
#endif  // DEBUG

static HeapObject AllocateUnaligned(NewSpace* space, int size) {
  AllocationResult allocation = space->AllocateRaw(size, kWordAligned);
  CHECK(!allocation.IsRetry());
  HeapObject filler;
  CHECK(allocation.To(&filler));
  space->heap()->CreateFillerObjectAt(filler.address(), size,
                                      ClearRecordedSlots::kNo);
  return filler;
}

static HeapObject AllocateUnaligned(PagedSpace* space, int size) {
  AllocationResult allocation = space->AllocateRaw(size, kWordAligned);
  CHECK(!allocation.IsRetry());
  HeapObject filler;
  CHECK(allocation.To(&filler));
  space->heap()->CreateFillerObjectAt(filler.address(), size,
                                      ClearRecordedSlots::kNo);
  return filler;
}

static HeapObject AllocateUnaligned(LargeObjectSpace* space, int size) {
  AllocationResult allocation = space->AllocateRaw(size);
  CHECK(!allocation.IsRetry());
  HeapObject filler;
  CHECK(allocation.To(&filler));
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

template <typename T>
void testAllocationObserver(Isolate* i_isolate, T* space) {
  Observer observer1(128);
  space->AddAllocationObserver(&observer1);

  // The observer should not get notified if we have only allocated less than
  // 128 bytes.
  AllocateUnaligned(space, 64);
  CHECK_EQ(observer1.count(), 0);

  // The observer should get called when we have allocated exactly 128 bytes.
  AllocateUnaligned(space, 64);
  CHECK_EQ(observer1.count(), 1);

  // Another >128 bytes should get another notification.
  AllocateUnaligned(space, 136);
  CHECK_EQ(observer1.count(), 2);

  // Allocating a large object should get only one notification.
  AllocateUnaligned(space, 1024);
  CHECK_EQ(observer1.count(), 3);

  // Allocating another 2048 bytes in small objects should get 16
  // notifications.
  for (int i = 0; i < 64; ++i) {
    AllocateUnaligned(space, 32);
  }
  CHECK_EQ(observer1.count(), 19);

  // Multiple observers should work.
  Observer observer2(96);
  space->AddAllocationObserver(&observer2);

  AllocateUnaligned(space, 2048);
  CHECK_EQ(observer1.count(), 20);
  CHECK_EQ(observer2.count(), 1);

  AllocateUnaligned(space, 104);
  CHECK_EQ(observer1.count(), 20);
  CHECK_EQ(observer2.count(), 2);

  // Callback should stop getting called after an observer is removed.
  space->RemoveAllocationObserver(&observer1);

  AllocateUnaligned(space, 384);
  CHECK_EQ(observer1.count(), 20);  // no more notifications.
  CHECK_EQ(observer2.count(), 3);   // this one is still active.

  // Ensure that PauseInlineAllocationObserversScope work correctly.
  AllocateUnaligned(space, 48);
  CHECK_EQ(observer2.count(), 3);
  {
    PauseAllocationObserversScope pause_observers(i_isolate->heap());
    CHECK_EQ(observer2.count(), 3);
    AllocateUnaligned(space, 384);
    CHECK_EQ(observer2.count(), 3);
  }
  CHECK_EQ(observer2.count(), 3);
  // Coupled with the 48 bytes allocated before the pause, another 48 bytes
  // allocated here should trigger a notification.
  AllocateUnaligned(space, 48);
  CHECK_EQ(observer2.count(), 4);

  space->RemoveAllocationObserver(&observer2);
  AllocateUnaligned(space, 384);
  CHECK_EQ(observer1.count(), 20);
  CHECK_EQ(observer2.count(), 4);
}

UNINITIALIZED_TEST(AllocationObserver) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();

    Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

    testAllocationObserver<NewSpace>(i_isolate, i_isolate->heap()->new_space());
    // Old space is used but the code path is shared for all
    // classes inheriting from PagedSpace.
    testAllocationObserver<PagedSpace>(i_isolate,
                                       i_isolate->heap()->old_space());
    testAllocationObserver<LargeObjectSpace>(i_isolate,
                                             i_isolate->heap()->lo_space());
  }
  isolate->Dispose();
}

UNINITIALIZED_TEST(InlineAllocationObserverCadence) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();

    Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

    // Clear out any pre-existing garbage to make the test consistent
    // across snapshot/no-snapshot builds.
    CcTest::CollectAllGarbage(i_isolate);

    NewSpace* new_space = i_isolate->heap()->new_space();

    Observer observer1(512);
    new_space->AddAllocationObserver(&observer1);
    Observer observer2(576);
    new_space->AddAllocationObserver(&observer2);

    for (int i = 0; i < 512; ++i) {
      AllocateUnaligned(new_space, 32);
    }

    new_space->RemoveAllocationObserver(&observer1);
    new_space->RemoveAllocationObserver(&observer2);

    CHECK_EQ(observer1.count(), 32);
    CHECK_EQ(observer2.count(), 28);
  }
  isolate->Dispose();
}

HEAP_TEST(Regress777177) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  PagedSpace* old_space = heap->old_space();
  Observer observer(128);
  old_space->AddAllocationObserver(&observer);

  int area_size = old_space->AreaSize();
  int max_object_size = kMaxRegularHeapObjectSize;
  int filler_size = area_size - max_object_size;

  {
    // Ensure a new linear allocation area on a fresh page.
    AlwaysAllocateScope always_allocate(isolate);
    heap::SimulateFullSpace(old_space);
    AllocationResult result = old_space->AllocateRaw(filler_size, kWordAligned);
    HeapObject obj = result.ToObjectChecked();
    heap->CreateFillerObjectAt(obj.address(), filler_size,
                               ClearRecordedSlots::kNo);
  }

  {
    // Allocate all bytes of the linear allocation area. This moves top_ and
    // top_on_previous_step_ to the next page.
    AllocationResult result =
        old_space->AllocateRaw(max_object_size, kWordAligned);
    HeapObject obj = result.ToObjectChecked();
    // Simulate allocation folding moving the top pointer back.
    old_space->SetTopAndLimit(obj.address(), old_space->limit());
  }

  {
    // This triggers assert in crbug.com/777177.
    AllocationResult result = old_space->AllocateRaw(filler_size, kWordAligned);
    HeapObject obj = result.ToObjectChecked();
    heap->CreateFillerObjectAt(obj.address(), filler_size,
                               ClearRecordedSlots::kNo);
  }
  old_space->RemoveAllocationObserver(&observer);
}

HEAP_TEST(Regress791582) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  NewSpace* new_space = heap->new_space();
  if (new_space->TotalCapacity() < new_space->MaximumCapacity()) {
    new_space->Grow();
  }

  int until_page_end = static_cast<int>(new_space->limit() - new_space->top());

  if (!IsAligned(until_page_end, kTaggedSize)) {
    // The test works if the size of allocation area size is a multiple of
    // pointer size. This is usually the case unless some allocation observer
    // is already active (e.g. incremental marking observer).
    return;
  }

  Observer observer(128);
  new_space->AddAllocationObserver(&observer);

  {
    AllocationResult result =
        new_space->AllocateRaw(until_page_end, kWordAligned);
    HeapObject obj = result.ToObjectChecked();
    heap->CreateFillerObjectAt(obj.address(), until_page_end,
                               ClearRecordedSlots::kNo);
    // Simulate allocation folding moving the top pointer back.
    *new_space->allocation_top_address() = obj.address();
  }

  {
    // This triggers assert in crbug.com/791582
    AllocationResult result = new_space->AllocateRaw(256, kWordAligned);
    HeapObject obj = result.ToObjectChecked();
    heap->CreateFillerObjectAt(obj.address(), 256, ClearRecordedSlots::kNo);
  }
  new_space->RemoveAllocationObserver(&observer);
}

TEST(ShrinkPageToHighWaterMarkFreeSpaceEnd) {
  FLAG_stress_incremental_marking = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  heap::SealCurrentObjects(CcTest::heap());

  // Prepare page that only contains a single object and a trailing FreeSpace
  // filler.
  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(128, AllocationType::kOld);
  Page* page = Page::FromHeapObject(*array);

  // Reset space so high water mark is consistent.
  PagedSpace* old_space = CcTest::heap()->old_space();
  old_space->FreeLinearAllocationArea();
  old_space->ResetFreeList();

  HeapObject filler = HeapObject::FromAddress(array->address() + array->Size());
  CHECK(filler.IsFreeSpace());
  size_t shrunk = old_space->ShrinkPageToHighWaterMark(page);
  size_t should_have_shrunk = RoundDown(
      static_cast<size_t>(MemoryChunkLayout::AllocatableMemoryInDataPage() -
                          array->Size()),
      CommitPageSize());
  CHECK_EQ(should_have_shrunk, shrunk);
}

TEST(ShrinkPageToHighWaterMarkNoFiller) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  heap::SealCurrentObjects(CcTest::heap());

  const int kFillerSize = 0;
  std::vector<Handle<FixedArray>> arrays =
      heap::FillOldSpacePageWithFixedArrays(CcTest::heap(), kFillerSize);
  Handle<FixedArray> array = arrays.back();
  Page* page = Page::FromHeapObject(*array);
  CHECK_EQ(page->area_end(), array->address() + array->Size() + kFillerSize);

  // Reset space so high water mark and fillers are consistent.
  PagedSpace* old_space = CcTest::heap()->old_space();
  old_space->ResetFreeList();
  old_space->FreeLinearAllocationArea();

  size_t shrunk = old_space->ShrinkPageToHighWaterMark(page);
  CHECK_EQ(0u, shrunk);
}

TEST(ShrinkPageToHighWaterMarkOneWordFiller) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  heap::SealCurrentObjects(CcTest::heap());

  const int kFillerSize = kTaggedSize;
  std::vector<Handle<FixedArray>> arrays =
      heap::FillOldSpacePageWithFixedArrays(CcTest::heap(), kFillerSize);
  Handle<FixedArray> array = arrays.back();
  Page* page = Page::FromHeapObject(*array);
  CHECK_EQ(page->area_end(), array->address() + array->Size() + kFillerSize);

  // Reset space so high water mark and fillers are consistent.
  PagedSpace* old_space = CcTest::heap()->old_space();
  old_space->FreeLinearAllocationArea();
  old_space->ResetFreeList();

  HeapObject filler = HeapObject::FromAddress(array->address() + array->Size());
  CHECK_EQ(filler.map(),
           ReadOnlyRoots(CcTest::heap()).one_pointer_filler_map());

  size_t shrunk = old_space->ShrinkPageToHighWaterMark(page);
  CHECK_EQ(0u, shrunk);
}

TEST(ShrinkPageToHighWaterMarkTwoWordFiller) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  heap::SealCurrentObjects(CcTest::heap());

  const int kFillerSize = 2 * kTaggedSize;
  std::vector<Handle<FixedArray>> arrays =
      heap::FillOldSpacePageWithFixedArrays(CcTest::heap(), kFillerSize);
  Handle<FixedArray> array = arrays.back();
  Page* page = Page::FromHeapObject(*array);
  CHECK_EQ(page->area_end(), array->address() + array->Size() + kFillerSize);

  // Reset space so high water mark and fillers are consistent.
  PagedSpace* old_space = CcTest::heap()->old_space();
  old_space->FreeLinearAllocationArea();
  old_space->ResetFreeList();

  HeapObject filler = HeapObject::FromAddress(array->address() + array->Size());
  CHECK_EQ(filler.map(),
           ReadOnlyRoots(CcTest::heap()).two_pointer_filler_map());

  size_t shrunk = old_space->ShrinkPageToHighWaterMark(page);
  CHECK_EQ(0u, shrunk);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
