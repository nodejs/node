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

#include "v8.h"
#include "cctest.h"

using namespace v8::internal;

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
  p->heap_ = HEAP;
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


namespace v8 {
namespace internal {

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

} }  // namespace v8::internal


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
  size_t alignment = code_range->exists() ?
                     MemoryChunk::kAlignment : OS::CommitPageSize();
  size_t reserved_size = ((executable == EXECUTABLE))
      ? RoundUp(header_size + guard_size + reserve_area_size + guard_size,
                alignment)
      : RoundUp(header_size + reserve_area_size, OS::CommitPageSize());
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


static unsigned int Pseudorandom() {
  static uint32_t lo = 2345;
  lo = 18273 * (lo & 0xFFFFF) + (lo >> 16);
  return lo & 0xFFFFF;
}


TEST(MemoryChunk) {
  Isolate* isolate = Isolate::Current();
  isolate->InitializeLoggingAndCounters();
  Heap* heap = isolate->heap();
  CHECK(heap->ConfigureHeapDefault());

  size_t reserve_area_size = 1 * MB;
  size_t initial_commit_area_size, second_commit_area_size;

  for (int i = 0; i < 100; i++) {
    initial_commit_area_size = Pseudorandom();
    second_commit_area_size = Pseudorandom();

    // With CodeRange.
    CodeRange* code_range = new CodeRange(isolate);
    const int code_range_size = 32 * MB;
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
  Isolate* isolate = Isolate::Current();
  isolate->InitializeLoggingAndCounters();
  Heap* heap = isolate->heap();
  CHECK(isolate->heap()->ConfigureHeapDefault());

  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(memory_allocator->SetUp(heap->MaxReserved(),
                                heap->MaxExecutableSize()));

  int total_pages = 0;
  OldSpace faked_space(heap,
                       heap->MaxReserved(),
                       OLD_POINTER_SPACE,
                       NOT_EXECUTABLE);
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
  Page* other = memory_allocator->AllocatePage(
      faked_space.AreaSize(), &faked_space, NOT_EXECUTABLE);
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
  memory_allocator->Free(first_page);
  memory_allocator->Free(second_page);
  memory_allocator->TearDown();
  delete memory_allocator;
}


TEST(NewSpace) {
  Isolate* isolate = Isolate::Current();
  isolate->InitializeLoggingAndCounters();
  Heap* heap = isolate->heap();
  CHECK(heap->ConfigureHeapDefault());
  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(memory_allocator->SetUp(heap->MaxReserved(),
                                heap->MaxExecutableSize()));
  TestMemoryAllocatorScope test_scope(isolate, memory_allocator);

  NewSpace new_space(heap);

  CHECK(new_space.SetUp(HEAP->ReservedSemiSpaceSize(),
                        HEAP->ReservedSemiSpaceSize()));
  CHECK(new_space.HasBeenSetUp());

  while (new_space.Available() >= Page::kMaxNonCodeHeapObjectSize) {
    Object* obj =
        new_space.AllocateRaw(Page::kMaxNonCodeHeapObjectSize)->
        ToObjectUnchecked();
    CHECK(new_space.Contains(HeapObject::cast(obj)));
  }

  new_space.TearDown();
  memory_allocator->TearDown();
  delete memory_allocator;
}


TEST(OldSpace) {
  Isolate* isolate = Isolate::Current();
  isolate->InitializeLoggingAndCounters();
  Heap* heap = isolate->heap();
  CHECK(heap->ConfigureHeapDefault());
  MemoryAllocator* memory_allocator = new MemoryAllocator(isolate);
  CHECK(memory_allocator->SetUp(heap->MaxReserved(),
                                heap->MaxExecutableSize()));
  TestMemoryAllocatorScope test_scope(isolate, memory_allocator);

  OldSpace* s = new OldSpace(heap,
                             heap->MaxOldGenerationSize(),
                             OLD_POINTER_SPACE,
                             NOT_EXECUTABLE);
  CHECK(s != NULL);

  CHECK(s->SetUp());

  while (s->Available() > 0) {
    s->AllocateRaw(Page::kMaxNonCodeHeapObjectSize)->ToObjectUnchecked();
  }

  s->TearDown();
  delete s;
  memory_allocator->TearDown();
  delete memory_allocator;
}


TEST(LargeObjectSpace) {
  v8::V8::Initialize();

  LargeObjectSpace* lo = HEAP->lo_space();
  CHECK(lo != NULL);

  int lo_size = Page::kPageSize;

  Object* obj = lo->AllocateRaw(lo_size, NOT_EXECUTABLE)->ToObjectUnchecked();
  CHECK(obj->IsHeapObject());

  HeapObject* ho = HeapObject::cast(obj);

  CHECK(lo->Contains(HeapObject::cast(obj)));

  CHECK(lo->FindObject(ho->address()) == obj);

  CHECK(lo->Contains(ho));

  while (true) {
    intptr_t available = lo->Available();
    { MaybeObject* maybe_obj = lo->AllocateRaw(lo_size, NOT_EXECUTABLE);
      if (!maybe_obj->ToObject(&obj)) break;
    }
    CHECK(lo->Available() < available);
  };

  CHECK(!lo->IsEmpty());

  CHECK(lo->AllocateRaw(lo_size, NOT_EXECUTABLE)->IsFailure());
}
