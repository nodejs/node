// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

static void VerifyRegionMarking(Address page_start) {
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
}


TEST(Page) {
  byte* mem = NewArray<byte>(2*Page::kPageSize);
  CHECK(mem != NULL);

  Address start = reinterpret_cast<Address>(mem);
  Address page_start = RoundUp(start, Page::kPageSize);

  Page* p = Page::FromAddress(page_start);
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


TEST(MemoryAllocator) {
  CHECK(Heap::ConfigureHeapDefault());
  CHECK(MemoryAllocator::Setup(Heap::MaxReserved(), Heap::MaxExecutableSize()));

  OldSpace faked_space(Heap::MaxReserved(), OLD_POINTER_SPACE, NOT_EXECUTABLE);
  int total_pages = 0;
  int requested = MemoryAllocator::kPagesPerChunk;
  int allocated;
  // If we request n pages, we should get n or n - 1.
  Page* first_page =
      MemoryAllocator::AllocatePages(requested, &allocated, &faked_space);
  CHECK(first_page->is_valid());
  CHECK(allocated == requested || allocated == requested - 1);
  total_pages += allocated;

  Page* last_page = first_page;
  for (Page* p = first_page; p->is_valid(); p = p->next_page()) {
    CHECK(MemoryAllocator::IsPageInSpace(p, &faked_space));
    last_page = p;
  }

  // Again, we should get n or n - 1 pages.
  Page* others =
      MemoryAllocator::AllocatePages(requested, &allocated, &faked_space);
  CHECK(others->is_valid());
  CHECK(allocated == requested || allocated == requested - 1);
  total_pages += allocated;

  MemoryAllocator::SetNextPage(last_page, others);
  int page_count = 0;
  for (Page* p = first_page; p->is_valid(); p = p->next_page()) {
    CHECK(MemoryAllocator::IsPageInSpace(p, &faked_space));
    page_count++;
  }
  CHECK(total_pages == page_count);

  Page* second_page = first_page->next_page();
  CHECK(second_page->is_valid());

  // Freeing pages at the first chunk starting at or after the second page
  // should free the entire second chunk.  It will return the page it was passed
  // (since the second page was in the first chunk).
  Page* free_return = MemoryAllocator::FreePages(second_page);
  CHECK(free_return == second_page);
  MemoryAllocator::SetNextPage(first_page, free_return);

  // Freeing pages in the first chunk starting at the first page should free
  // the first chunk and return an invalid page.
  Page* invalid_page = MemoryAllocator::FreePages(first_page);
  CHECK(!invalid_page->is_valid());

  MemoryAllocator::TearDown();
}


TEST(NewSpace) {
  CHECK(Heap::ConfigureHeapDefault());
  CHECK(MemoryAllocator::Setup(Heap::MaxReserved(), Heap::MaxExecutableSize()));

  NewSpace new_space;

  void* chunk =
      MemoryAllocator::ReserveInitialChunk(4 * Heap::ReservedSemiSpaceSize());
  CHECK(chunk != NULL);
  Address start = RoundUp(static_cast<Address>(chunk),
                          2 * Heap::ReservedSemiSpaceSize());
  CHECK(new_space.Setup(start, 2 * Heap::ReservedSemiSpaceSize()));
  CHECK(new_space.HasBeenSetup());

  while (new_space.Available() >= Page::kMaxHeapObjectSize) {
    Object* obj =
        new_space.AllocateRaw(Page::kMaxHeapObjectSize)->ToObjectUnchecked();
    CHECK(new_space.Contains(HeapObject::cast(obj)));
  }

  new_space.TearDown();
  MemoryAllocator::TearDown();
}


TEST(OldSpace) {
  CHECK(Heap::ConfigureHeapDefault());
  CHECK(MemoryAllocator::Setup(Heap::MaxReserved(), Heap::MaxExecutableSize()));

  OldSpace* s = new OldSpace(Heap::MaxOldGenerationSize(),
                             OLD_POINTER_SPACE,
                             NOT_EXECUTABLE);
  CHECK(s != NULL);

  void* chunk =
      MemoryAllocator::ReserveInitialChunk(4 * Heap::ReservedSemiSpaceSize());
  CHECK(chunk != NULL);
  Address start = static_cast<Address>(chunk);
  size_t size = RoundUp(start, 2 * Heap::ReservedSemiSpaceSize()) - start;

  CHECK(s->Setup(start, size));

  while (s->Available() > 0) {
    s->AllocateRaw(Page::kMaxHeapObjectSize)->ToObjectUnchecked();
  }

  s->TearDown();
  delete s;
  MemoryAllocator::TearDown();
}


TEST(LargeObjectSpace) {
  CHECK(Heap::Setup(false));

  LargeObjectSpace* lo = Heap::lo_space();
  CHECK(lo != NULL);

  Map* faked_map = reinterpret_cast<Map*>(HeapObject::FromAddress(0));
  int lo_size = Page::kPageSize;

  Object* obj = lo->AllocateRaw(lo_size)->ToObjectUnchecked();
  CHECK(obj->IsHeapObject());

  HeapObject* ho = HeapObject::cast(obj);
  ho->set_map(faked_map);

  CHECK(lo->Contains(HeapObject::cast(obj)));

  CHECK(lo->FindObject(ho->address()) == obj);

  CHECK(lo->Contains(ho));

  while (true) {
    intptr_t available = lo->Available();
    { MaybeObject* maybe_obj = lo->AllocateRaw(lo_size);
      if (!maybe_obj->ToObject(&obj)) break;
    }
    HeapObject::cast(obj)->set_map(faked_map);
    CHECK(lo->Available() < available);
  };

  CHECK(!lo->IsEmpty());

  CHECK(lo->AllocateRaw(lo_size)->IsFailure());

  lo->TearDown();
  delete lo;

  MemoryAllocator::TearDown();
}
