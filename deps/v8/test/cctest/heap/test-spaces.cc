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
#include "src/snapshot/snapshot.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/utils-inl.h"

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
      : isolate_(isolate),
        old_allocator_(isolate->memory_allocator_) {
    isolate->memory_allocator_ = allocator;
  }

  ~TestMemoryAllocatorScope() {
    isolate_->memory_allocator_ = old_allocator_;
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
        old_code_range_(isolate->code_range_) {
    isolate->code_range_ = code_range;
  }

  ~TestCodeRangeScope() {
    isolate_->code_range_ = old_code_range_;
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
  CHECK(memory_allocator->SetUp(heap->MaxReserved(),
                                heap->MaxExecutableSize()));
  TestMemoryAllocatorScope test_allocator_scope(isolate, memory_allocator);
  TestCodeRangeScope test_code_range_scope(isolate, code_range);

  size_t header_size = (executable == EXECUTABLE)
                       ? MemoryAllocator::CodePageGuardStartOffset()
                       : MemoryChunk::kObjectStartOffset;
  size_t guard_size = (executable == EXECUTABLE)
                       ? MemoryAllocator::CodePageGuardSize()
                       : 0;

  MemoryChunk* memory_chunk = memory_allocator->AllocateChunk(reserve_area_size,
                                                              commit_area_size,
                                                              executable,
                                                              NULL);
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
  CHECK(memory_chunk->area_start() < memory_chunk->address() +
                                     memory_chunk->size());
  CHECK(memory_chunk->area_end() <= memory_chunk->address() +
                                    memory_chunk->size());
  CHECK(static_cast<size_t>(memory_chunk->area_size()) == commit_area_size);

  Address area_start = memory_chunk->area_start();

  memory_chunk->CommitArea(second_commit_area_size);
  CHECK(area_start == memory_chunk->area_start());
  CHECK(memory_chunk->area_start() < memory_chunk->address() +
                                     memory_chunk->size());
  CHECK(memory_chunk->area_end() <= memory_chunk->address() +
                                    memory_chunk->size());
  CHECK(static_cast<size_t>(memory_chunk->area_size()) ==
      second_commit_area_size);

  memory_allocator->Free(memory_chunk);
  memory_allocator->TearDown();
  delete memory_allocator;
}


TEST(Regress3540) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  const int pageSize = Page::kPageSize;
  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(
      memory_allocator->SetUp(heap->MaxReserved(), heap->MaxExecutableSize()));
  TestMemoryAllocatorScope test_allocator_scope(isolate, memory_allocator);
  CodeRange* code_range = new CodeRange(isolate);
  const size_t code_range_size = 4 * pageSize;
  if (!code_range->SetUp(
          code_range_size +
          RoundUp(v8::base::OS::CommitPageSize() * kReservedCodeRangePages,
                  MemoryChunk::kAlignment) +
          v8::internal::MemoryAllocator::CodePageAreaSize())) {
    return;
  }

  Address address;
  size_t size;
  size_t request_size = code_range_size - 2 * pageSize;
  address = code_range->AllocateRawMemory(
      request_size, request_size - (2 * MemoryAllocator::CodePageGuardSize()),
      &size);
  CHECK(address != NULL);

  Address null_address;
  size_t null_size;
  request_size = code_range_size - pageSize;
  null_address = code_range->AllocateRawMemory(
      request_size, request_size - (2 * MemoryAllocator::CodePageGuardSize()),
      &null_size);
  CHECK(null_address == NULL);

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

    // Without CodeRange.
    code_range = NULL;
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
  }
}


TEST(MemoryAllocator) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(memory_allocator != nullptr);
  CHECK(memory_allocator->SetUp(heap->MaxReserved(),
                                heap->MaxExecutableSize()));
  TestMemoryAllocatorScope test_scope(isolate, memory_allocator);

  {
    int total_pages = 0;
    OldSpace faked_space(heap, OLD_SPACE, NOT_EXECUTABLE);
    Page* first_page = memory_allocator->AllocatePage(
        faked_space.AreaSize(), &faked_space, NOT_EXECUTABLE);

    first_page->InsertAfter(faked_space.anchor()->prev_page());
    CHECK(first_page->is_valid());
    CHECK(first_page->next_page() == faked_space.anchor());
    total_pages++;

    for (Page* p = first_page; p != faked_space.anchor(); p = p->next_page()) {
      CHECK(p->owner() == &faked_space);
    }

    // Again, we should get n or n - 1 pages.
    Page* other = memory_allocator->AllocatePage(faked_space.AreaSize(),
                                                 &faked_space, NOT_EXECUTABLE);
    CHECK(other->is_valid());
    total_pages++;
    other->InsertAfter(first_page);
    int page_count = 0;
    for (Page* p = first_page; p != faked_space.anchor(); p = p->next_page()) {
      CHECK(p->owner() == &faked_space);
      page_count++;
    }
    CHECK(total_pages == page_count);

    Page* second_page = first_page->next_page();
    CHECK(second_page->is_valid());

    // OldSpace's destructor will tear down the space and free up all pages.
  }
  memory_allocator->TearDown();
  delete memory_allocator;
}


TEST(NewSpace) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(memory_allocator->SetUp(heap->MaxReserved(),
                                heap->MaxExecutableSize()));
  TestMemoryAllocatorScope test_scope(isolate, memory_allocator);

  NewSpace new_space(heap);

  CHECK(new_space.SetUp(CcTest::heap()->ReservedSemiSpaceSize(),
                        CcTest::heap()->ReservedSemiSpaceSize()));
  CHECK(new_space.HasBeenSetUp());

  while (new_space.Available() >= Page::kMaxRegularHeapObjectSize) {
    Object* obj =
        new_space.AllocateRawUnaligned(Page::kMaxRegularHeapObjectSize)
            .ToObjectChecked();
    CHECK(new_space.Contains(HeapObject::cast(obj)));
  }

  new_space.TearDown();
  memory_allocator->TearDown();
  delete memory_allocator;
}


TEST(OldSpace) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(memory_allocator->SetUp(heap->MaxReserved(),
                                heap->MaxExecutableSize()));
  TestMemoryAllocatorScope test_scope(isolate, memory_allocator);

  OldSpace* s = new OldSpace(heap, OLD_SPACE, NOT_EXECUTABLE);
  CHECK(s != NULL);

  CHECK(s->SetUp());

  while (s->Available() > 0) {
    s->AllocateRawUnaligned(Page::kMaxRegularHeapObjectSize).ToObjectChecked();
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
  CHECK(
      memory_allocator->SetUp(heap->MaxReserved(), heap->MaxExecutableSize()));
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
      compaction_space->AreaSize() / Page::kMaxRegularHeapObjectSize;
  const int kExpectedPages =
      (kNumObjects + kNumObjectsPerPage - 1) / kNumObjectsPerPage;
  for (int i = 0; i < kNumObjects; i++) {
    compaction_space->AllocateRawUnaligned(Page::kMaxRegularHeapObjectSize)
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


TEST(CompactionSpaceUsingExternalMemory) {
  const int kObjectSize = 512;

  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  MemoryAllocator* allocator = new MemoryAllocator(isolate);
  CHECK(allocator != nullptr);
  CHECK(allocator->SetUp(heap->MaxReserved(), heap->MaxExecutableSize()));
  TestMemoryAllocatorScope test_scope(isolate, allocator);

  CompactionSpaceCollection* collection = new CompactionSpaceCollection(heap);
  CompactionSpace* compaction_space = collection->Get(OLD_SPACE);
  CHECK(compaction_space != NULL);
  CHECK(compaction_space->SetUp());

  OldSpace* old_space = new OldSpace(heap, OLD_SPACE, NOT_EXECUTABLE);
  CHECK(old_space != NULL);
  CHECK(old_space->SetUp());

  // The linear allocation area already counts as used bytes, making
  // exact testing impossible.
  heap->DisableInlineAllocation();

  // Test:
  // * Allocate a backing store in old_space.
  // * Compute the number num_rest_objects of kObjectSize objects that fit into
  //   of available memory.
  //   kNumRestObjects.
  // * Add the rest of available memory to the compaction space.
  // * Allocate kNumRestObjects in the compaction space.
  // * Allocate one object more.
  // * Merge the compaction space and compare the expected number of pages.

  // Allocate a single object in old_space to initialize a backing page.
  old_space->AllocateRawUnaligned(kObjectSize).ToObjectChecked();
  // Compute the number of objects that fit into the rest in old_space.
  intptr_t rest = static_cast<int>(old_space->Available());
  CHECK_GT(rest, 0);
  intptr_t num_rest_objects = rest / kObjectSize;
  // After allocating num_rest_objects in compaction_space we allocate a bit
  // more.
  const intptr_t kAdditionalCompactionMemory = kObjectSize;
  // We expect a single old_space page.
  const intptr_t kExpectedInitialOldSpacePages = 1;
  // We expect a single additional page in compaction space because we mostly
  // use external memory.
  const intptr_t kExpectedCompactionPages = 1;
  // We expect two pages to be reachable from old_space in the end.
  const intptr_t kExpectedOldSpacePagesAfterMerge = 2;

  CHECK_EQ(old_space->CountTotalPages(), kExpectedInitialOldSpacePages);
  CHECK_EQ(compaction_space->CountTotalPages(), 0);
  CHECK_EQ(compaction_space->Capacity(), 0);
  // Make the rest of memory available for compaction.
  old_space->DivideUponCompactionSpaces(&collection, 1, rest);
  CHECK_EQ(compaction_space->CountTotalPages(), 0);
  CHECK_EQ(compaction_space->Capacity(), rest);
  while (num_rest_objects-- > 0) {
    compaction_space->AllocateRawUnaligned(kObjectSize).ToObjectChecked();
  }
  // We only used external memory so far.
  CHECK_EQ(compaction_space->CountTotalPages(), 0);
  // Additional allocation.
  compaction_space->AllocateRawUnaligned(kAdditionalCompactionMemory)
      .ToObjectChecked();
  // Now the compaction space shouldve also acquired a page.
  CHECK_EQ(compaction_space->CountTotalPages(), kExpectedCompactionPages);

  old_space->MergeCompactionSpace(compaction_space);
  CHECK_EQ(old_space->CountTotalPages(), kExpectedOldSpacePagesAfterMerge);

  delete collection;
  delete old_space;

  allocator->TearDown();
  delete allocator;
}


CompactionSpaceCollection** HeapTester::InitializeCompactionSpaces(
    Heap* heap, int num_spaces) {
  CompactionSpaceCollection** spaces =
      new CompactionSpaceCollection*[num_spaces];
  for (int i = 0; i < num_spaces; i++) {
    spaces[i] = new CompactionSpaceCollection(heap);
  }
  return spaces;
}


void HeapTester::DestroyCompactionSpaces(CompactionSpaceCollection** spaces,
                                         int num_spaces) {
  for (int i = 0; i < num_spaces; i++) {
    delete spaces[i];
  }
  delete[] spaces;
}


void HeapTester::MergeCompactionSpaces(PagedSpace* space,
                                       CompactionSpaceCollection** spaces,
                                       int num_spaces) {
  AllocationSpace id = space->identity();
  for (int i = 0; i < num_spaces; i++) {
    space->MergeCompactionSpace(spaces[i]->Get(id));
    CHECK_EQ(spaces[i]->Get(id)->accounting_stats_.Size(), 0);
    CHECK_EQ(spaces[i]->Get(id)->accounting_stats_.Capacity(), 0);
    CHECK_EQ(spaces[i]->Get(id)->Waste(), 0);
  }
}


void HeapTester::AllocateInCompactionSpaces(CompactionSpaceCollection** spaces,
                                            AllocationSpace id, int num_spaces,
                                            int num_objects, int object_size) {
  for (int i = 0; i < num_spaces; i++) {
    for (int j = 0; j < num_objects; j++) {
      spaces[i]->Get(id)->AllocateRawUnaligned(object_size).ToObjectChecked();
    }
    spaces[i]->Get(id)->EmptyAllocationInfo();
    CHECK_EQ(spaces[i]->Get(id)->accounting_stats_.Size(),
             num_objects * object_size);
    CHECK_GE(spaces[i]->Get(id)->accounting_stats_.Capacity(),
             spaces[i]->Get(id)->accounting_stats_.Size());
  }
}


void HeapTester::CompactionStats(CompactionSpaceCollection** spaces,
                                 AllocationSpace id, int num_spaces,
                                 intptr_t* capacity, intptr_t* size) {
  *capacity = 0;
  *size = 0;
  for (int i = 0; i < num_spaces; i++) {
    *capacity += spaces[i]->Get(id)->accounting_stats_.Capacity();
    *size += spaces[i]->Get(id)->accounting_stats_.Size();
  }
}


void HeapTester::TestCompactionSpaceDivide(int num_additional_objects,
                                           int object_size,
                                           int num_compaction_spaces,
                                           int additional_capacity_in_bytes) {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  OldSpace* old_space = new OldSpace(heap, OLD_SPACE, NOT_EXECUTABLE);
  CHECK(old_space != nullptr);
  CHECK(old_space->SetUp());
  old_space->AllocateRawUnaligned(object_size).ToObjectChecked();
  old_space->EmptyAllocationInfo();

  intptr_t rest_capacity = old_space->accounting_stats_.Capacity() -
                           old_space->accounting_stats_.Size();
  intptr_t capacity_for_compaction_space =
      rest_capacity / num_compaction_spaces;
  int num_objects_in_compaction_space =
      static_cast<int>(capacity_for_compaction_space) / object_size +
      num_additional_objects;
  CHECK_GT(num_objects_in_compaction_space, 0);
  intptr_t initial_old_space_capacity = old_space->accounting_stats_.Capacity();

  CompactionSpaceCollection** spaces =
      InitializeCompactionSpaces(heap, num_compaction_spaces);
  old_space->DivideUponCompactionSpaces(spaces, num_compaction_spaces,
                                        capacity_for_compaction_space);

  intptr_t compaction_capacity = 0;
  intptr_t compaction_size = 0;
  CompactionStats(spaces, OLD_SPACE, num_compaction_spaces,
                  &compaction_capacity, &compaction_size);

  intptr_t old_space_capacity = old_space->accounting_stats_.Capacity();
  intptr_t old_space_size = old_space->accounting_stats_.Size();
  // Compaction space memory is subtracted from the original space's capacity.
  CHECK_EQ(old_space_capacity,
           initial_old_space_capacity - compaction_capacity);
  CHECK_EQ(compaction_size, 0);

  AllocateInCompactionSpaces(spaces, OLD_SPACE, num_compaction_spaces,
                             num_objects_in_compaction_space, object_size);

  // Old space size and capacity should be the same as after dividing.
  CHECK_EQ(old_space->accounting_stats_.Size(), old_space_size);
  CHECK_EQ(old_space->accounting_stats_.Capacity(), old_space_capacity);

  CompactionStats(spaces, OLD_SPACE, num_compaction_spaces,
                  &compaction_capacity, &compaction_size);
  MergeCompactionSpaces(old_space, spaces, num_compaction_spaces);

  CHECK_EQ(old_space->accounting_stats_.Capacity(),
           old_space_capacity + compaction_capacity);
  CHECK_EQ(old_space->accounting_stats_.Size(),
           old_space_size + compaction_size);
  // We check against the expected end capacity.
  CHECK_EQ(old_space->accounting_stats_.Capacity(),
           initial_old_space_capacity + additional_capacity_in_bytes);

  DestroyCompactionSpaces(spaces, num_compaction_spaces);
  delete old_space;
}


HEAP_TEST(CompactionSpaceDivideSinglePage) {
  const int kObjectSize = KB;
  const int kCompactionSpaces = 4;
  // Since the bound for objects is tight and the dividing is best effort, we
  // subtract some objects to make sure we still fit in the initial page.
  // A CHECK makes sure that the overall number of allocated objects stays
  // > 0.
  const int kAdditionalObjects = -10;
  const int kAdditionalCapacityRequired = 0;
  TestCompactionSpaceDivide(kAdditionalObjects, kObjectSize, kCompactionSpaces,
                            kAdditionalCapacityRequired);
}


HEAP_TEST(CompactionSpaceDivideMultiplePages) {
  const int kObjectSize = KB;
  const int kCompactionSpaces = 4;
  // Allocate half a page of objects to ensure that we need one more page per
  // compaction space.
  const int kAdditionalObjects = (Page::kPageSize / kObjectSize / 2);
  const int kAdditionalCapacityRequired =
      Page::kAllocatableMemory * kCompactionSpaces;
  TestCompactionSpaceDivide(kAdditionalObjects, kObjectSize, kCompactionSpaces,
                            kAdditionalCapacityRequired);
}


TEST(LargeObjectSpace) {
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


TEST(SizeOfFirstPageIsLargeEnough) {
  if (i::FLAG_always_opt) return;
  // Bootstrapping without a snapshot causes more allocations.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  if (!isolate->snapshot_available()) return;
  if (Snapshot::EmbedsScript(isolate)) return;

  // If this test fails due to enabling experimental natives that are not part
  // of the snapshot, we may need to adjust CalculateFirstPageSizes.

  // Freshly initialized VM gets by with one page per space.
  for (int i = FIRST_PAGED_SPACE; i <= LAST_PAGED_SPACE; i++) {
    // Debug code can be very large, so skip CODE_SPACE if we are generating it.
    if (i == CODE_SPACE && i::FLAG_debug_code) continue;
    CHECK_EQ(1, isolate->heap()->paged_space(i)->CountTotalPages());
  }

  // Executing the empty script gets by with one page per space.
  HandleScope scope(isolate);
  CompileRun("/*empty*/");
  for (int i = FIRST_PAGED_SPACE; i <= LAST_PAGED_SPACE; i++) {
    // Debug code can be very large, so skip CODE_SPACE if we are generating it.
    if (i == CODE_SPACE && i::FLAG_debug_code) continue;
    CHECK_EQ(1, isolate->heap()->paged_space(i)->CountTotalPages());
  }

  // No large objects required to perform the above steps.
  CHECK(isolate->heap()->lo_space()->IsEmpty());
}


UNINITIALIZED_TEST(NewSpaceGrowsToTargetCapacity) {
  FLAG_target_semi_space_size = 2 * (Page::kPageSize / MB);
  if (FLAG_optimize_for_size) return;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();

    Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

    NewSpace* new_space = i_isolate->heap()->new_space();

    // This test doesn't work if we start with a non-default new space
    // configuration.
    if (new_space->InitialTotalCapacity() == Page::kPageSize) {
      CHECK_EQ(new_space->CommittedMemory(), new_space->InitialTotalCapacity());

      // Fill up the first (and only) page of the semi space.
      FillCurrentPage(new_space);

      // Try to allocate out of the new space. A new page should be added and
      // the
      // allocation should succeed.
      v8::internal::AllocationResult allocation =
          new_space->AllocateRawUnaligned(80);
      CHECK(!allocation.IsRetry());
      CHECK_EQ(new_space->CommittedMemory(), 2 * Page::kPageSize);

      // Turn the allocation into a proper object so isolate teardown won't
      // crash.
      HeapObject* free_space = NULL;
      CHECK(allocation.To(&free_space));
      new_space->heap()->CreateFillerObjectAt(free_space->address(), 80);
    }
  }
  isolate->Dispose();
}


static HeapObject* AllocateUnaligned(NewSpace* space, int size) {
  AllocationResult allocation = space->AllocateRawUnaligned(size);
  CHECK(!allocation.IsRetry());
  HeapObject* filler = NULL;
  CHECK(allocation.To(&filler));
  space->heap()->CreateFillerObjectAt(filler->address(), size);
  return filler;
}

class Observer : public InlineAllocationObserver {
 public:
  explicit Observer(intptr_t step_size)
      : InlineAllocationObserver(step_size), count_(0) {}

  void Step(int bytes_allocated, Address, size_t) override { count_++; }

  int count() const { return count_; }

 private:
  int count_;
};


UNINITIALIZED_TEST(InlineAllocationObserver) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();

    Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

    NewSpace* new_space = i_isolate->heap()->new_space();

    Observer observer1(128);
    new_space->AddInlineAllocationObserver(&observer1);

    // The observer should not get notified if we have only allocated less than
    // 128 bytes.
    AllocateUnaligned(new_space, 64);
    CHECK_EQ(observer1.count(), 0);

    // The observer should get called when we have allocated exactly 128 bytes.
    AllocateUnaligned(new_space, 64);
    CHECK_EQ(observer1.count(), 1);

    // Another >128 bytes should get another notification.
    AllocateUnaligned(new_space, 136);
    CHECK_EQ(observer1.count(), 2);

    // Allocating a large object should get only one notification.
    AllocateUnaligned(new_space, 1024);
    CHECK_EQ(observer1.count(), 3);

    // Allocating another 2048 bytes in small objects should get 16
    // notifications.
    for (int i = 0; i < 64; ++i) {
      AllocateUnaligned(new_space, 32);
    }
    CHECK_EQ(observer1.count(), 19);

    // Multiple observers should work.
    Observer observer2(96);
    new_space->AddInlineAllocationObserver(&observer2);

    AllocateUnaligned(new_space, 2048);
    CHECK_EQ(observer1.count(), 20);
    CHECK_EQ(observer2.count(), 1);

    AllocateUnaligned(new_space, 104);
    CHECK_EQ(observer1.count(), 20);
    CHECK_EQ(observer2.count(), 2);

    // Callback should stop getting called after an observer is removed.
    new_space->RemoveInlineAllocationObserver(&observer1);

    AllocateUnaligned(new_space, 384);
    CHECK_EQ(observer1.count(), 20);  // no more notifications.
    CHECK_EQ(observer2.count(), 3);   // this one is still active.

    // Ensure that PauseInlineAllocationObserversScope work correctly.
    AllocateUnaligned(new_space, 48);
    CHECK_EQ(observer2.count(), 3);
    {
      PauseInlineAllocationObserversScope pause_observers(new_space);
      CHECK_EQ(observer2.count(), 3);
      AllocateUnaligned(new_space, 384);
      CHECK_EQ(observer2.count(), 3);
    }
    CHECK_EQ(observer2.count(), 3);
    // Coupled with the 48 bytes allocated before the pause, another 48 bytes
    // allocated here should trigger a notification.
    AllocateUnaligned(new_space, 48);
    CHECK_EQ(observer2.count(), 4);

    new_space->RemoveInlineAllocationObserver(&observer2);
    AllocateUnaligned(new_space, 384);
    CHECK_EQ(observer1.count(), 20);
    CHECK_EQ(observer2.count(), 4);
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
    new_space->AddInlineAllocationObserver(&observer1);
    Observer observer2(576);
    new_space->AddInlineAllocationObserver(&observer2);

    for (int i = 0; i < 512; ++i) {
      AllocateUnaligned(new_space, 32);
    }

    new_space->RemoveInlineAllocationObserver(&observer1);
    new_space->RemoveInlineAllocationObserver(&observer2);

    CHECK_EQ(observer1.count(), 32);
    CHECK_EQ(observer2.count(), 28);
  }
  isolate->Dispose();
}

}  // namespace internal
}  // namespace v8
