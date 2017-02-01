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

#include "src/base/platform/platform.h"
#include "src/heap/spaces-inl.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/heap/incremental-marking.h -> src/objects-inl.h
#include "src/objects-inl.h"
#include "src/snapshot/snapshot.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {

#if 0
static void VerifyRegionMarking(Address page_start) {
#ifdef ENABLE_CARDMARKING_WRITE_BARRIER
  Page* p = Page::FromAddress(page_start);

  p->SetRegionMarks(Page::kAllRegionsCleanMarks);

  for (Address addr = p->ObjectAreaStart();
       addr < p->ObjectAreaEnd();
       addr += kPointerSize) {
    CHECK(!Page::FromAddress(addr)->IsRegionDirty(addr));
  }

  for (Address addr = p->ObjectAreaStart();
       addr < p->ObjectAreaEnd();
       addr += kPointerSize) {
    Page::FromAddress(addr)->MarkRegionDirty(addr);
  }

  for (Address addr = p->ObjectAreaStart();
       addr < p->ObjectAreaEnd();
       addr += kPointerSize) {
    CHECK(Page::FromAddress(addr)->IsRegionDirty(addr));
  }
#endif
}
#endif


// TODO(gc) you can no longer allocate pages like this. Details are hidden.
#if 0
TEST(Page) {
  byte* mem = NewArray<byte>(2*Page::kPageSize);
  CHECK(mem != NULL);

  Address start = reinterpret_cast<Address>(mem);
  Address page_start = RoundUp(start, Page::kPageSize);

  Page* p = Page::FromAddress(page_start);
  // Initialized Page has heap pointer, normally set by memory_allocator.
  p->heap_ = CcTest::heap();
  CHECK(p->address() == page_start);
  CHECK(p->is_valid());

  p->opaque_header = 0;
  p->SetIsLargeObjectPage(false);
  CHECK(!p->next_page()->is_valid());

  CHECK(p->ObjectAreaStart() == page_start + Page::kObjectStartOffset);
  CHECK(p->ObjectAreaEnd() == page_start + Page::kPageSize);

  CHECK(p->Offset(page_start + Page::kObjectStartOffset) ==
        Page::kObjectStartOffset);
  CHECK(p->Offset(page_start + Page::kPageSize) == Page::kPageSize);

  CHECK(p->OffsetToAddress(Page::kObjectStartOffset) == p->ObjectAreaStart());
  CHECK(p->OffsetToAddress(Page::kPageSize) == p->ObjectAreaEnd());

  // test region marking
  VerifyRegionMarking(page_start);

  DeleteArray(mem);
}
#endif


// Temporarily sets a given allocator in an isolate.
class TestMemoryAllocatorScope {
 public:
  TestMemoryAllocatorScope(Isolate* isolate, MemoryAllocator* allocator)
      : isolate_(isolate), old_allocator_(isolate->heap()->memory_allocator()) {
    isolate->heap()->memory_allocator_ = allocator;
  }

  ~TestMemoryAllocatorScope() {
    isolate_->heap()->memory_allocator_ = old_allocator_;
  }

 private:
  Isolate* isolate_;
  MemoryAllocator* old_allocator_;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryAllocatorScope);
};


// Temporarily sets a given code range in an isolate.
class TestCodeRangeScope {
 public:
  TestCodeRangeScope(Isolate* isolate, CodeRange* code_range)
      : isolate_(isolate),
        old_code_range_(isolate->heap()->memory_allocator()->code_range()) {
    isolate->heap()->memory_allocator()->code_range_ = code_range;
  }

  ~TestCodeRangeScope() {
    isolate_->heap()->memory_allocator()->code_range_ = old_code_range_;
  }

 private:
  Isolate* isolate_;
  CodeRange* old_code_range_;

  DISALLOW_COPY_AND_ASSIGN(TestCodeRangeScope);
};


static void VerifyMemoryChunk(Isolate* isolate,
                              Heap* heap,
                              CodeRange* code_range,
                              size_t reserve_area_size,
                              size_t commit_area_size,
                              size_t second_commit_area_size,
                              Executability executable) {
  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(memory_allocator->SetUp(heap->MaxReserved(), heap->MaxExecutableSize(),
                                0));
  {
    TestMemoryAllocatorScope test_allocator_scope(isolate, memory_allocator);
    TestCodeRangeScope test_code_range_scope(isolate, code_range);

    size_t header_size = (executable == EXECUTABLE)
                             ? MemoryAllocator::CodePageGuardStartOffset()
                             : MemoryChunk::kObjectStartOffset;
    size_t guard_size =
        (executable == EXECUTABLE) ? MemoryAllocator::CodePageGuardSize() : 0;

    MemoryChunk* memory_chunk = memory_allocator->AllocateChunk(
        reserve_area_size, commit_area_size, executable, NULL);
    size_t alignment = code_range != NULL && code_range->valid()
                           ? MemoryChunk::kAlignment
                           : base::OS::CommitPageSize();
    size_t reserved_size =
        ((executable == EXECUTABLE))
            ? RoundUp(header_size + guard_size + reserve_area_size + guard_size,
                      alignment)
            : RoundUp(header_size + reserve_area_size,
                      base::OS::CommitPageSize());
    CHECK(memory_chunk->size() == reserved_size);
    CHECK(memory_chunk->area_start() <
          memory_chunk->address() + memory_chunk->size());
    CHECK(memory_chunk->area_end() <=
          memory_chunk->address() + memory_chunk->size());
    CHECK(static_cast<size_t>(memory_chunk->area_size()) == commit_area_size);

    Address area_start = memory_chunk->area_start();

    memory_chunk->CommitArea(second_commit_area_size);
    CHECK(area_start == memory_chunk->area_start());
    CHECK(memory_chunk->area_start() <
          memory_chunk->address() + memory_chunk->size());
    CHECK(memory_chunk->area_end() <=
          memory_chunk->address() + memory_chunk->size());
    CHECK(static_cast<size_t>(memory_chunk->area_size()) ==
          second_commit_area_size);

    memory_allocator->Free<MemoryAllocator::kFull>(memory_chunk);
  }
  memory_allocator->TearDown();
  delete memory_allocator;
}


TEST(Regress3540) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(memory_allocator->SetUp(heap->MaxReserved(), heap->MaxExecutableSize(),
                                0));
  TestMemoryAllocatorScope test_allocator_scope(isolate, memory_allocator);
  CodeRange* code_range = new CodeRange(isolate);
  size_t code_range_size =
      kMinimumCodeRangeSize > 0 ? kMinimumCodeRangeSize : 3 * Page::kPageSize;
  if (!code_range->SetUp(code_range_size)) {
    return;
  }

  Address address;
  size_t size;
  size_t request_size = code_range_size - Page::kPageSize;
  address = code_range->AllocateRawMemory(
      request_size, request_size - (2 * MemoryAllocator::CodePageGuardSize()),
      &size);
  CHECK_NOT_NULL(address);

  Address null_address;
  size_t null_size;
  request_size = code_range_size - Page::kPageSize;
  null_address = code_range->AllocateRawMemory(
      request_size, request_size - (2 * MemoryAllocator::CodePageGuardSize()),
      &null_size);
  CHECK_NULL(null_address);

  code_range->FreeRawMemory(address, size);
  delete code_range;
  memory_allocator->TearDown();
  delete memory_allocator;
}


static unsigned int Pseudorandom() {
  static uint32_t lo = 2345;
  lo = 18273 * (lo & 0xFFFFF) + (lo >> 16);
  return lo & 0xFFFFF;
}


TEST(MemoryChunk) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  size_t reserve_area_size = 1 * MB;
  size_t initial_commit_area_size, second_commit_area_size;

  for (int i = 0; i < 100; i++) {
    initial_commit_area_size = Pseudorandom();
    second_commit_area_size = Pseudorandom();

    // With CodeRange.
    CodeRange* code_range = new CodeRange(isolate);
    const size_t code_range_size = 32 * MB;
    if (!code_range->SetUp(code_range_size)) return;

    VerifyMemoryChunk(isolate,
                      heap,
                      code_range,
                      reserve_area_size,
                      initial_commit_area_size,
                      second_commit_area_size,
                      EXECUTABLE);

    VerifyMemoryChunk(isolate,
                      heap,
                      code_range,
                      reserve_area_size,
                      initial_commit_area_size,
                      second_commit_area_size,
                      NOT_EXECUTABLE);
    delete code_range;

    // Without a valid CodeRange, i.e., omitting SetUp.
    code_range = new CodeRange(isolate);
    VerifyMemoryChunk(isolate,
                      heap,
                      code_range,
                      reserve_area_size,
                      initial_commit_area_size,
                      second_commit_area_size,
                      EXECUTABLE);

    VerifyMemoryChunk(isolate,
                      heap,
                      code_range,
                      reserve_area_size,
                      initial_commit_area_size,
                      second_commit_area_size,
                      NOT_EXECUTABLE);
    delete code_range;
  }
}


TEST(MemoryAllocator) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(memory_allocator != nullptr);
  CHECK(memory_allocator->SetUp(heap->MaxReserved(), heap->MaxExecutableSize(),
                                0));
  TestMemoryAllocatorScope test_scope(isolate, memory_allocator);

  {
    int total_pages = 0;
    OldSpace faked_space(heap, OLD_SPACE, NOT_EXECUTABLE);
    Page* first_page = memory_allocator->AllocatePage(
        faked_space.AreaSize(), static_cast<PagedSpace*>(&faked_space),
        NOT_EXECUTABLE);

    first_page->InsertAfter(faked_space.anchor()->prev_page());
    CHECK(Page::IsValid(first_page));
    CHECK(first_page->next_page() == faked_space.anchor());
    total_pages++;

    for (Page* p = first_page; p != faked_space.anchor(); p = p->next_page()) {
      CHECK(p->owner() == &faked_space);
    }

    // Again, we should get n or n - 1 pages.
    Page* other = memory_allocator->AllocatePage(
        faked_space.AreaSize(), static_cast<PagedSpace*>(&faked_space),
        NOT_EXECUTABLE);
    CHECK(Page::IsValid(other));
    total_pages++;
    other->InsertAfter(first_page);
    int page_count = 0;
    for (Page* p = first_page; p != faked_space.anchor(); p = p->next_page()) {
      CHECK(p->owner() == &faked_space);
      page_count++;
    }
    CHECK(total_pages == page_count);

    Page* second_page = first_page->next_page();
    CHECK(Page::IsValid(second_page));

    // OldSpace's destructor will tear down the space and free up all pages.
  }
  memory_allocator->TearDown();
  delete memory_allocator;
}


TEST(NewSpace) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(memory_allocator->SetUp(heap->MaxReserved(), heap->MaxExecutableSize(),
                                0));
  TestMemoryAllocatorScope test_scope(isolate, memory_allocator);

  NewSpace new_space(heap);

  CHECK(new_space.SetUp(CcTest::heap()->InitialSemiSpaceSize(),
                        CcTest::heap()->InitialSemiSpaceSize()));
  CHECK(new_space.HasBeenSetUp());

  while (new_space.Available() >= kMaxRegularHeapObjectSize) {
    CHECK(new_space.Contains(
        new_space.AllocateRawUnaligned(kMaxRegularHeapObjectSize)
            .ToObjectChecked()));
  }

  new_space.TearDown();
  memory_allocator->TearDown();
  delete memory_allocator;
}


TEST(OldSpace) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(memory_allocator->SetUp(heap->MaxReserved(), heap->MaxExecutableSize(),
                                0));
  TestMemoryAllocatorScope test_scope(isolate, memory_allocator);

  OldSpace* s = new OldSpace(heap, OLD_SPACE, NOT_EXECUTABLE);
  CHECK(s != NULL);

  CHECK(s->SetUp());

  while (s->Available() > 0) {
    s->AllocateRawUnaligned(kMaxRegularHeapObjectSize).ToObjectChecked();
  }

  delete s;
  memory_allocator->TearDown();
  delete memory_allocator;
}


TEST(CompactionSpace) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(memory_allocator != nullptr);
  CHECK(memory_allocator->SetUp(heap->MaxReserved(), heap->MaxExecutableSize(),
                                0));
  TestMemoryAllocatorScope test_scope(isolate, memory_allocator);

  CompactionSpace* compaction_space =
      new CompactionSpace(heap, OLD_SPACE, NOT_EXECUTABLE);
  CHECK(compaction_space != NULL);
  CHECK(compaction_space->SetUp());

  OldSpace* old_space = new OldSpace(heap, OLD_SPACE, NOT_EXECUTABLE);
  CHECK(old_space != NULL);
  CHECK(old_space->SetUp());

  // Cannot loop until "Available()" since we initially have 0 bytes available
  // and would thus neither grow, nor be able to allocate an object.
  const int kNumObjects = 100;
  const int kNumObjectsPerPage =
      compaction_space->AreaSize() / kMaxRegularHeapObjectSize;
  const int kExpectedPages =
      (kNumObjects + kNumObjectsPerPage - 1) / kNumObjectsPerPage;
  for (int i = 0; i < kNumObjects; i++) {
    compaction_space->AllocateRawUnaligned(kMaxRegularHeapObjectSize)
        .ToObjectChecked();
  }
  int pages_in_old_space = old_space->CountTotalPages();
  int pages_in_compaction_space = compaction_space->CountTotalPages();
  CHECK_EQ(pages_in_compaction_space, kExpectedPages);
  CHECK_LE(pages_in_old_space, 1);

  old_space->MergeCompactionSpace(compaction_space);
  CHECK_EQ(old_space->CountTotalPages(),
           pages_in_old_space + pages_in_compaction_space);

  delete compaction_space;
  delete old_space;

  memory_allocator->TearDown();
  delete memory_allocator;
}


TEST(LargeObjectSpace) {
  // This test does not initialize allocated objects, which confuses the
  // incremental marker.
  FLAG_incremental_marking = false;
  v8::V8::Initialize();

  LargeObjectSpace* lo = CcTest::heap()->lo_space();
  CHECK(lo != NULL);

  int lo_size = Page::kPageSize;

  Object* obj = lo->AllocateRaw(lo_size, NOT_EXECUTABLE).ToObjectChecked();
  CHECK(obj->IsHeapObject());

  HeapObject* ho = HeapObject::cast(obj);

  CHECK(lo->Contains(HeapObject::cast(obj)));

  CHECK(lo->FindObject(ho->address()) == obj);

  CHECK(lo->Contains(ho));

  while (true) {
    intptr_t available = lo->Available();
    { AllocationResult allocation = lo->AllocateRaw(lo_size, NOT_EXECUTABLE);
      if (allocation.IsRetry()) break;
    }
    // The available value is conservative such that it may report
    // zero prior to heap exhaustion.
    CHECK(lo->Available() < available || available == 0);
  }

  CHECK(!lo->IsEmpty());

  CHECK(lo->AllocateRaw(lo_size, NOT_EXECUTABLE).IsRetry());
}

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
  const size_t kMaxInitialSizePerSpace = 2 * MB;

  // Freshly initialized VM gets by with the snapshot size (which is below
  // kMaxInitialSizePerSpace per space).
  Heap* heap = isolate->heap();
  int page_count[LAST_PAGED_SPACE + 1] = {0, 0, 0, 0};
  for (int i = FIRST_PAGED_SPACE; i <= LAST_PAGED_SPACE; i++) {
    // Debug code can be very large, so skip CODE_SPACE if we are generating it.
    if (i == CODE_SPACE && i::FLAG_debug_code) continue;

    page_count[i] = heap->paged_space(i)->CountTotalPages();
    // Check that the initial heap is also below the limit.
    CHECK_LT(heap->paged_space(i)->CommittedMemory(), kMaxInitialSizePerSpace);
  }

  // Executing the empty script gets by with the same number of pages, i.e.,
  // requires no extra space.
  CompileRun("/*empty*/");
  for (int i = FIRST_PAGED_SPACE; i <= LAST_PAGED_SPACE; i++) {
    // Debug code can be very large, so skip CODE_SPACE if we are generating it.
    if (i == CODE_SPACE && i::FLAG_debug_code) continue;
    CHECK_EQ(page_count[i], isolate->heap()->paged_space(i)->CountTotalPages());
  }

  // No large objects required to perform the above steps.
  CHECK_EQ(initial_lo_space, isolate->heap()->lo_space()->Size());
}

static HeapObject* AllocateUnaligned(NewSpace* space, int size) {
  AllocationResult allocation = space->AllocateRawUnaligned(size);
  CHECK(!allocation.IsRetry());
  HeapObject* filler = NULL;
  CHECK(allocation.To(&filler));
  space->heap()->CreateFillerObjectAt(filler->address(), size,
                                      ClearRecordedSlots::kNo);
  return filler;
}

static HeapObject* AllocateUnaligned(PagedSpace* space, int size) {
  AllocationResult allocation = space->AllocateRaw(size, kDoubleUnaligned);
  CHECK(!allocation.IsRetry());
  HeapObject* filler = NULL;
  CHECK(allocation.To(&filler));
  space->heap()->CreateFillerObjectAt(filler->address(), size,
                                      ClearRecordedSlots::kNo);
  return filler;
}

static HeapObject* AllocateUnaligned(LargeObjectSpace* space, int size) {
  AllocationResult allocation = space->AllocateRaw(size, EXECUTABLE);
  CHECK(!allocation.IsRetry());
  HeapObject* filler = NULL;
  CHECK(allocation.To(&filler));
  return filler;
}

class Observer : public AllocationObserver {
 public:
  explicit Observer(intptr_t step_size)
      : AllocationObserver(step_size), count_(0) {}

  void Step(int bytes_allocated, Address, size_t) override { count_++; }

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

TEST(ShrinkPageToHighWaterMarkFreeSpaceEnd) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  heap::SealCurrentObjects(CcTest::heap());

  // Prepare page that only contains a single object and a trailing FreeSpace
  // filler.
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(128, TENURED);
  Page* page = Page::FromAddress(array->address());

  // Reset space so high water mark is consistent.
  CcTest::heap()->old_space()->ResetFreeList();
  CcTest::heap()->old_space()->EmptyAllocationInfo();

  HeapObject* filler =
      HeapObject::FromAddress(array->address() + array->Size());
  CHECK(filler->IsFreeSpace());
  size_t shrinked = page->ShrinkToHighWaterMark();
  size_t should_have_shrinked =
      RoundDown(static_cast<size_t>(Page::kAllocatableMemory - array->Size()),
                base::OS::CommitPageSize());
  CHECK_EQ(should_have_shrinked, shrinked);
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
  Page* page = Page::FromAddress(array->address());
  CHECK_EQ(page->area_end(), array->address() + array->Size() + kFillerSize);

  // Reset space so high water mark and fillers are consistent.
  CcTest::heap()->old_space()->ResetFreeList();
  CcTest::heap()->old_space()->EmptyAllocationInfo();

  const size_t shrinked = page->ShrinkToHighWaterMark();
  CHECK_EQ(0, shrinked);
}

TEST(ShrinkPageToHighWaterMarkOneWordFiller) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  heap::SealCurrentObjects(CcTest::heap());

  const int kFillerSize = kPointerSize;
  std::vector<Handle<FixedArray>> arrays =
      heap::FillOldSpacePageWithFixedArrays(CcTest::heap(), kFillerSize);
  Handle<FixedArray> array = arrays.back();
  Page* page = Page::FromAddress(array->address());
  CHECK_EQ(page->area_end(), array->address() + array->Size() + kFillerSize);

  // Reset space so high water mark and fillers are consistent.
  CcTest::heap()->old_space()->ResetFreeList();
  CcTest::heap()->old_space()->EmptyAllocationInfo();

  HeapObject* filler =
      HeapObject::FromAddress(array->address() + array->Size());
  CHECK_EQ(filler->map(), CcTest::heap()->one_pointer_filler_map());

  const size_t shrinked = page->ShrinkToHighWaterMark();
  CHECK_EQ(0, shrinked);
}

TEST(ShrinkPageToHighWaterMarkTwoWordFiller) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  heap::SealCurrentObjects(CcTest::heap());

  const int kFillerSize = 2 * kPointerSize;
  std::vector<Handle<FixedArray>> arrays =
      heap::FillOldSpacePageWithFixedArrays(CcTest::heap(), kFillerSize);
  Handle<FixedArray> array = arrays.back();
  Page* page = Page::FromAddress(array->address());
  CHECK_EQ(page->area_end(), array->address() + array->Size() + kFillerSize);

  // Reset space so high water mark and fillers are consistent.
  CcTest::heap()->old_space()->ResetFreeList();
  CcTest::heap()->old_space()->EmptyAllocationInfo();

  HeapObject* filler =
      HeapObject::FromAddress(array->address() + array->Size());
  CHECK_EQ(filler->map(), CcTest::heap()->two_pointer_filler_map());

  const size_t shrinked = page->ShrinkToHighWaterMark();
  CHECK_EQ(0, shrinked);
}

}  // namespace internal
}  // namespace v8
