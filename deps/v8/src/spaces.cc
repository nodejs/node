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

#include "v8.h"

#include "liveobjectlist-inl.h"
#include "macro-assembler.h"
#include "mark-compact.h"
#include "platform.h"

namespace v8 {
namespace internal {

// For contiguous spaces, top should be in the space (or at the end) and limit
// should be the end of the space.
#define ASSERT_SEMISPACE_ALLOCATION_INFO(info, space) \
  ASSERT((space).low() <= (info).top                  \
         && (info).top <= (space).high()              \
         && (info).limit == (space).high())

// ----------------------------------------------------------------------------
// HeapObjectIterator

HeapObjectIterator::HeapObjectIterator(PagedSpace* space) {
  Initialize(space->bottom(), space->top(), NULL);
}


HeapObjectIterator::HeapObjectIterator(PagedSpace* space,
                                       HeapObjectCallback size_func) {
  Initialize(space->bottom(), space->top(), size_func);
}


HeapObjectIterator::HeapObjectIterator(PagedSpace* space, Address start) {
  Initialize(start, space->top(), NULL);
}


HeapObjectIterator::HeapObjectIterator(PagedSpace* space, Address start,
                                       HeapObjectCallback size_func) {
  Initialize(start, space->top(), size_func);
}


HeapObjectIterator::HeapObjectIterator(Page* page,
                                       HeapObjectCallback size_func) {
  Initialize(page->ObjectAreaStart(), page->AllocationTop(), size_func);
}


void HeapObjectIterator::Initialize(Address cur, Address end,
                                    HeapObjectCallback size_f) {
  cur_addr_ = cur;
  end_addr_ = end;
  end_page_ = Page::FromAllocationTop(end);
  size_func_ = size_f;
  Page* p = Page::FromAllocationTop(cur_addr_);
  cur_limit_ = (p == end_page_) ? end_addr_ : p->AllocationTop();

#ifdef DEBUG
  Verify();
#endif
}


HeapObject* HeapObjectIterator::FromNextPage() {
  if (cur_addr_ == end_addr_) return NULL;

  Page* cur_page = Page::FromAllocationTop(cur_addr_);
  cur_page = cur_page->next_page();
  ASSERT(cur_page->is_valid());

  cur_addr_ = cur_page->ObjectAreaStart();
  cur_limit_ = (cur_page == end_page_) ? end_addr_ : cur_page->AllocationTop();

  if (cur_addr_ == end_addr_) return NULL;
  ASSERT(cur_addr_ < cur_limit_);
#ifdef DEBUG
  Verify();
#endif
  return FromCurrentPage();
}


#ifdef DEBUG
void HeapObjectIterator::Verify() {
  Page* p = Page::FromAllocationTop(cur_addr_);
  ASSERT(p == Page::FromAllocationTop(cur_limit_));
  ASSERT(p->Offset(cur_addr_) <= p->Offset(cur_limit_));
}
#endif


// -----------------------------------------------------------------------------
// PageIterator

PageIterator::PageIterator(PagedSpace* space, Mode mode) : space_(space) {
  prev_page_ = NULL;
  switch (mode) {
    case PAGES_IN_USE:
      stop_page_ = space->AllocationTopPage();
      break;
    case PAGES_USED_BY_MC:
      stop_page_ = space->MCRelocationTopPage();
      break;
    case ALL_PAGES:
#ifdef DEBUG
      // Verify that the cached last page in the space is actually the
      // last page.
      for (Page* p = space->first_page_; p->is_valid(); p = p->next_page()) {
        if (!p->next_page()->is_valid()) {
          ASSERT(space->last_page_ == p);
        }
      }
#endif
      stop_page_ = space->last_page_;
      break;
  }
}


// -----------------------------------------------------------------------------
// CodeRange


CodeRange::CodeRange(Isolate* isolate)
    : isolate_(isolate),
      code_range_(NULL),
      free_list_(0),
      allocation_list_(0),
      current_allocation_block_index_(0) {
}


bool CodeRange::Setup(const size_t requested) {
  ASSERT(code_range_ == NULL);

  code_range_ = new VirtualMemory(requested);
  CHECK(code_range_ != NULL);
  if (!code_range_->IsReserved()) {
    delete code_range_;
    code_range_ = NULL;
    return false;
  }

  // We are sure that we have mapped a block of requested addresses.
  ASSERT(code_range_->size() == requested);
  LOG(isolate_, NewEvent("CodeRange", code_range_->address(), requested));
  allocation_list_.Add(FreeBlock(code_range_->address(), code_range_->size()));
  current_allocation_block_index_ = 0;
  return true;
}


int CodeRange::CompareFreeBlockAddress(const FreeBlock* left,
                                       const FreeBlock* right) {
  // The entire point of CodeRange is that the difference between two
  // addresses in the range can be represented as a signed 32-bit int,
  // so the cast is semantically correct.
  return static_cast<int>(left->start - right->start);
}


void CodeRange::GetNextAllocationBlock(size_t requested) {
  for (current_allocation_block_index_++;
       current_allocation_block_index_ < allocation_list_.length();
       current_allocation_block_index_++) {
    if (requested <= allocation_list_[current_allocation_block_index_].size) {
      return;  // Found a large enough allocation block.
    }
  }

  // Sort and merge the free blocks on the free list and the allocation list.
  free_list_.AddAll(allocation_list_);
  allocation_list_.Clear();
  free_list_.Sort(&CompareFreeBlockAddress);
  for (int i = 0; i < free_list_.length();) {
    FreeBlock merged = free_list_[i];
    i++;
    // Add adjacent free blocks to the current merged block.
    while (i < free_list_.length() &&
           free_list_[i].start == merged.start + merged.size) {
      merged.size += free_list_[i].size;
      i++;
    }
    if (merged.size > 0) {
      allocation_list_.Add(merged);
    }
  }
  free_list_.Clear();

  for (current_allocation_block_index_ = 0;
       current_allocation_block_index_ < allocation_list_.length();
       current_allocation_block_index_++) {
    if (requested <= allocation_list_[current_allocation_block_index_].size) {
      return;  // Found a large enough allocation block.
    }
  }

  // Code range is full or too fragmented.
  V8::FatalProcessOutOfMemory("CodeRange::GetNextAllocationBlock");
}



void* CodeRange::AllocateRawMemory(const size_t requested, size_t* allocated) {
  ASSERT(current_allocation_block_index_ < allocation_list_.length());
  if (requested > allocation_list_[current_allocation_block_index_].size) {
    // Find an allocation block large enough.  This function call may
    // call V8::FatalProcessOutOfMemory if it cannot find a large enough block.
    GetNextAllocationBlock(requested);
  }
  // Commit the requested memory at the start of the current allocation block.
  *allocated = RoundUp(requested, Page::kPageSize);
  FreeBlock current = allocation_list_[current_allocation_block_index_];
  if (*allocated >= current.size - Page::kPageSize) {
    // Don't leave a small free block, useless for a large object or chunk.
    *allocated = current.size;
  }
  ASSERT(*allocated <= current.size);
  if (!code_range_->Commit(current.start, *allocated, true)) {
    *allocated = 0;
    return NULL;
  }
  allocation_list_[current_allocation_block_index_].start += *allocated;
  allocation_list_[current_allocation_block_index_].size -= *allocated;
  if (*allocated == current.size) {
    GetNextAllocationBlock(0);  // This block is used up, get the next one.
  }
  return current.start;
}


void CodeRange::FreeRawMemory(void* address, size_t length) {
  free_list_.Add(FreeBlock(address, length));
  code_range_->Uncommit(address, length);
}


void CodeRange::TearDown() {
    delete code_range_;  // Frees all memory in the virtual memory range.
    code_range_ = NULL;
    free_list_.Free();
    allocation_list_.Free();
}


// -----------------------------------------------------------------------------
// MemoryAllocator
//

// 270 is an estimate based on the static default heap size of a pair of 256K
// semispaces and a 64M old generation.
const int kEstimatedNumberOfChunks = 270;


MemoryAllocator::MemoryAllocator(Isolate* isolate)
    : isolate_(isolate),
      capacity_(0),
      capacity_executable_(0),
      size_(0),
      size_executable_(0),
      initial_chunk_(NULL),
      chunks_(kEstimatedNumberOfChunks),
      free_chunk_ids_(kEstimatedNumberOfChunks),
      max_nof_chunks_(0),
      top_(0) {
}


void MemoryAllocator::Push(int free_chunk_id) {
  ASSERT(max_nof_chunks_ > 0);
  ASSERT(top_ < max_nof_chunks_);
  free_chunk_ids_[top_++] = free_chunk_id;
}


int MemoryAllocator::Pop() {
  ASSERT(top_ > 0);
  return free_chunk_ids_[--top_];
}


bool MemoryAllocator::Setup(intptr_t capacity, intptr_t capacity_executable) {
  capacity_ = RoundUp(capacity, Page::kPageSize);
  capacity_executable_ = RoundUp(capacity_executable, Page::kPageSize);
  ASSERT_GE(capacity_, capacity_executable_);

  // Over-estimate the size of chunks_ array.  It assumes the expansion of old
  // space is always in the unit of a chunk (kChunkSize) except the last
  // expansion.
  //
  // Due to alignment, allocated space might be one page less than required
  // number (kPagesPerChunk) of pages for old spaces.
  //
  // Reserve two chunk ids for semispaces, one for map space, one for old
  // space, and one for code space.
  max_nof_chunks_ =
      static_cast<int>((capacity_ / (kChunkSize - Page::kPageSize))) + 5;
  if (max_nof_chunks_ > kMaxNofChunks) return false;

  size_ = 0;
  size_executable_ = 0;
  ChunkInfo info;  // uninitialized element.
  for (int i = max_nof_chunks_ - 1; i >= 0; i--) {
    chunks_.Add(info);
    free_chunk_ids_.Add(i);
  }
  top_ = max_nof_chunks_;
  return true;
}


void MemoryAllocator::TearDown() {
  for (int i = 0; i < max_nof_chunks_; i++) {
    if (chunks_[i].address() != NULL) DeleteChunk(i);
  }
  chunks_.Clear();
  free_chunk_ids_.Clear();

  if (initial_chunk_ != NULL) {
    LOG(isolate_, DeleteEvent("InitialChunk", initial_chunk_->address()));
    delete initial_chunk_;
    initial_chunk_ = NULL;
  }

  ASSERT(top_ == max_nof_chunks_);  // all chunks are free
  top_ = 0;
  capacity_ = 0;
  capacity_executable_ = 0;
  size_ = 0;
  max_nof_chunks_ = 0;
}


void* MemoryAllocator::AllocateRawMemory(const size_t requested,
                                         size_t* allocated,
                                         Executability executable) {
  if (size_ + static_cast<size_t>(requested) > static_cast<size_t>(capacity_)) {
    return NULL;
  }

  void* mem;
  if (executable == EXECUTABLE) {
    // Check executable memory limit.
    if (size_executable_ + requested >
        static_cast<size_t>(capacity_executable_)) {
      LOG(isolate_,
          StringEvent("MemoryAllocator::AllocateRawMemory",
                      "V8 Executable Allocation capacity exceeded"));
      return NULL;
    }
    // Allocate executable memory either from code range or from the
    // OS.
    if (isolate_->code_range()->exists()) {
      mem = isolate_->code_range()->AllocateRawMemory(requested, allocated);
    } else {
      mem = OS::Allocate(requested, allocated, true);
    }
    // Update executable memory size.
    size_executable_ += static_cast<int>(*allocated);
  } else {
    mem = OS::Allocate(requested, allocated, false);
  }
  int alloced = static_cast<int>(*allocated);
  size_ += alloced;

#ifdef DEBUG
  ZapBlock(reinterpret_cast<Address>(mem), alloced);
#endif
  isolate_->counters()->memory_allocated()->Increment(alloced);
  return mem;
}


void MemoryAllocator::FreeRawMemory(void* mem,
                                    size_t length,
                                    Executability executable) {
#ifdef DEBUG
  // Do not try to zap the guard page.
  size_t guard_size = (executable == EXECUTABLE) ? Page::kPageSize : 0;
  ZapBlock(reinterpret_cast<Address>(mem) + guard_size, length - guard_size);
#endif
  if (isolate_->code_range()->contains(static_cast<Address>(mem))) {
    isolate_->code_range()->FreeRawMemory(mem, length);
  } else {
    OS::Free(mem, length);
  }
  isolate_->counters()->memory_allocated()->Decrement(static_cast<int>(length));
  size_ -= static_cast<int>(length);
  if (executable == EXECUTABLE) size_executable_ -= static_cast<int>(length);

  ASSERT(size_ >= 0);
  ASSERT(size_executable_ >= 0);
}


void MemoryAllocator::PerformAllocationCallback(ObjectSpace space,
                                                AllocationAction action,
                                                size_t size) {
  for (int i = 0; i < memory_allocation_callbacks_.length(); ++i) {
    MemoryAllocationCallbackRegistration registration =
      memory_allocation_callbacks_[i];
    if ((registration.space & space) == space &&
        (registration.action & action) == action)
      registration.callback(space, action, static_cast<int>(size));
  }
}


bool MemoryAllocator::MemoryAllocationCallbackRegistered(
    MemoryAllocationCallback callback) {
  for (int i = 0; i < memory_allocation_callbacks_.length(); ++i) {
    if (memory_allocation_callbacks_[i].callback == callback) return true;
  }
  return false;
}


void MemoryAllocator::AddMemoryAllocationCallback(
    MemoryAllocationCallback callback,
    ObjectSpace space,
    AllocationAction action) {
  ASSERT(callback != NULL);
  MemoryAllocationCallbackRegistration registration(callback, space, action);
  ASSERT(!MemoryAllocator::MemoryAllocationCallbackRegistered(callback));
  return memory_allocation_callbacks_.Add(registration);
}


void MemoryAllocator::RemoveMemoryAllocationCallback(
     MemoryAllocationCallback callback) {
  ASSERT(callback != NULL);
  for (int i = 0; i < memory_allocation_callbacks_.length(); ++i) {
    if (memory_allocation_callbacks_[i].callback == callback) {
      memory_allocation_callbacks_.Remove(i);
      return;
    }
  }
  UNREACHABLE();
}

void* MemoryAllocator::ReserveInitialChunk(const size_t requested) {
  ASSERT(initial_chunk_ == NULL);

  initial_chunk_ = new VirtualMemory(requested);
  CHECK(initial_chunk_ != NULL);
  if (!initial_chunk_->IsReserved()) {
    delete initial_chunk_;
    initial_chunk_ = NULL;
    return NULL;
  }

  // We are sure that we have mapped a block of requested addresses.
  ASSERT(initial_chunk_->size() == requested);
  LOG(isolate_,
      NewEvent("InitialChunk", initial_chunk_->address(), requested));
  size_ += static_cast<int>(requested);
  return initial_chunk_->address();
}


static int PagesInChunk(Address start, size_t size) {
  // The first page starts on the first page-aligned address from start onward
  // and the last page ends on the last page-aligned address before
  // start+size.  Page::kPageSize is a power of two so we can divide by
  // shifting.
  return static_cast<int>((RoundDown(start + size, Page::kPageSize)
      - RoundUp(start, Page::kPageSize)) >> kPageSizeBits);
}


Page* MemoryAllocator::AllocatePages(int requested_pages,
                                     int* allocated_pages,
                                     PagedSpace* owner) {
  if (requested_pages <= 0) return Page::FromAddress(NULL);
  size_t chunk_size = requested_pages * Page::kPageSize;

  void* chunk = AllocateRawMemory(chunk_size, &chunk_size, owner->executable());
  if (chunk == NULL) return Page::FromAddress(NULL);
  LOG(isolate_, NewEvent("PagedChunk", chunk, chunk_size));

  *allocated_pages = PagesInChunk(static_cast<Address>(chunk), chunk_size);

  // We may 'lose' a page due to alignment.
  ASSERT(*allocated_pages >= kPagesPerChunk - 1);

  size_t guard_size = (owner->executable() == EXECUTABLE) ? Page::kPageSize : 0;

  // Check that we got at least one page that we can use.
  if (*allocated_pages <= ((guard_size != 0) ? 1 : 0)) {
    FreeRawMemory(chunk,
                  chunk_size,
                  owner->executable());
    LOG(isolate_, DeleteEvent("PagedChunk", chunk));
    return Page::FromAddress(NULL);
  }

  if (guard_size != 0) {
    OS::Guard(chunk, guard_size);
    chunk_size -= guard_size;
    chunk = static_cast<Address>(chunk) + guard_size;
    --*allocated_pages;
  }

  int chunk_id = Pop();
  chunks_[chunk_id].init(static_cast<Address>(chunk), chunk_size, owner);

  ObjectSpace space = static_cast<ObjectSpace>(1 << owner->identity());
  PerformAllocationCallback(space, kAllocationActionAllocate, chunk_size);
  Page* new_pages = InitializePagesInChunk(chunk_id, *allocated_pages, owner);

  return new_pages;
}


Page* MemoryAllocator::CommitPages(Address start, size_t size,
                                   PagedSpace* owner, int* num_pages) {
  ASSERT(start != NULL);
  *num_pages = PagesInChunk(start, size);
  ASSERT(*num_pages > 0);
  ASSERT(initial_chunk_ != NULL);
  ASSERT(InInitialChunk(start));
  ASSERT(InInitialChunk(start + size - 1));
  if (!initial_chunk_->Commit(start, size, owner->executable() == EXECUTABLE)) {
    return Page::FromAddress(NULL);
  }
#ifdef DEBUG
  ZapBlock(start, size);
#endif
  isolate_->counters()->memory_allocated()->Increment(static_cast<int>(size));

  // So long as we correctly overestimated the number of chunks we should not
  // run out of chunk ids.
  CHECK(!OutOfChunkIds());
  int chunk_id = Pop();
  chunks_[chunk_id].init(start, size, owner);
  return InitializePagesInChunk(chunk_id, *num_pages, owner);
}


bool MemoryAllocator::CommitBlock(Address start,
                                  size_t size,
                                  Executability executable) {
  ASSERT(start != NULL);
  ASSERT(size > 0);
  ASSERT(initial_chunk_ != NULL);
  ASSERT(InInitialChunk(start));
  ASSERT(InInitialChunk(start + size - 1));

  if (!initial_chunk_->Commit(start, size, executable)) return false;
#ifdef DEBUG
  ZapBlock(start, size);
#endif
  isolate_->counters()->memory_allocated()->Increment(static_cast<int>(size));
  return true;
}


bool MemoryAllocator::UncommitBlock(Address start, size_t size) {
  ASSERT(start != NULL);
  ASSERT(size > 0);
  ASSERT(initial_chunk_ != NULL);
  ASSERT(InInitialChunk(start));
  ASSERT(InInitialChunk(start + size - 1));

  if (!initial_chunk_->Uncommit(start, size)) return false;
  isolate_->counters()->memory_allocated()->Decrement(static_cast<int>(size));
  return true;
}


void MemoryAllocator::ZapBlock(Address start, size_t size) {
  for (size_t s = 0; s + kPointerSize <= size; s += kPointerSize) {
    Memory::Address_at(start + s) = kZapValue;
  }
}


Page* MemoryAllocator::InitializePagesInChunk(int chunk_id, int pages_in_chunk,
                                              PagedSpace* owner) {
  ASSERT(IsValidChunk(chunk_id));
  ASSERT(pages_in_chunk > 0);

  Address chunk_start = chunks_[chunk_id].address();

  Address low = RoundUp(chunk_start, Page::kPageSize);

#ifdef DEBUG
  size_t chunk_size = chunks_[chunk_id].size();
  Address high = RoundDown(chunk_start + chunk_size, Page::kPageSize);
  ASSERT(pages_in_chunk <=
        ((OffsetFrom(high) - OffsetFrom(low)) / Page::kPageSize));
#endif

  Address page_addr = low;
  for (int i = 0; i < pages_in_chunk; i++) {
    Page* p = Page::FromAddress(page_addr);
    p->heap_ = owner->heap();
    p->opaque_header = OffsetFrom(page_addr + Page::kPageSize) | chunk_id;
    p->InvalidateWatermark(true);
    p->SetIsLargeObjectPage(false);
    p->SetAllocationWatermark(p->ObjectAreaStart());
    p->SetCachedAllocationWatermark(p->ObjectAreaStart());
    page_addr += Page::kPageSize;
  }

  // Set the next page of the last page to 0.
  Page* last_page = Page::FromAddress(page_addr - Page::kPageSize);
  last_page->opaque_header = OffsetFrom(0) | chunk_id;

  return Page::FromAddress(low);
}


Page* MemoryAllocator::FreePages(Page* p) {
  if (!p->is_valid()) return p;

  // Find the first page in the same chunk as 'p'
  Page* first_page = FindFirstPageInSameChunk(p);
  Page* page_to_return = Page::FromAddress(NULL);

  if (p != first_page) {
    // Find the last page in the same chunk as 'prev'.
    Page* last_page = FindLastPageInSameChunk(p);
    first_page = GetNextPage(last_page);  // first page in next chunk

    // set the next_page of last_page to NULL
    SetNextPage(last_page, Page::FromAddress(NULL));
    page_to_return = p;  // return 'p' when exiting
  }

  while (first_page->is_valid()) {
    int chunk_id = GetChunkId(first_page);
    ASSERT(IsValidChunk(chunk_id));

    // Find the first page of the next chunk before deleting this chunk.
    first_page = GetNextPage(FindLastPageInSameChunk(first_page));

    // Free the current chunk.
    DeleteChunk(chunk_id);
  }

  return page_to_return;
}


void MemoryAllocator::FreeAllPages(PagedSpace* space) {
  for (int i = 0, length = chunks_.length(); i < length; i++) {
    if (chunks_[i].owner() == space) {
      DeleteChunk(i);
    }
  }
}


void MemoryAllocator::DeleteChunk(int chunk_id) {
  ASSERT(IsValidChunk(chunk_id));

  ChunkInfo& c = chunks_[chunk_id];

  // We cannot free a chunk contained in the initial chunk because it was not
  // allocated with AllocateRawMemory.  Instead we uncommit the virtual
  // memory.
  if (InInitialChunk(c.address())) {
    // TODO(1240712): VirtualMemory::Uncommit has a return value which
    // is ignored here.
    initial_chunk_->Uncommit(c.address(), c.size());
    Counters* counters = isolate_->counters();
    counters->memory_allocated()->Decrement(static_cast<int>(c.size()));
  } else {
    LOG(isolate_, DeleteEvent("PagedChunk", c.address()));
    ObjectSpace space = static_cast<ObjectSpace>(1 << c.owner_identity());
    size_t size = c.size();
    size_t guard_size = (c.executable() == EXECUTABLE) ? Page::kPageSize : 0;
    FreeRawMemory(c.address() - guard_size, size + guard_size, c.executable());
    PerformAllocationCallback(space, kAllocationActionFree, size);
  }
  c.init(NULL, 0, NULL);
  Push(chunk_id);
}


Page* MemoryAllocator::FindFirstPageInSameChunk(Page* p) {
  int chunk_id = GetChunkId(p);
  ASSERT(IsValidChunk(chunk_id));

  Address low = RoundUp(chunks_[chunk_id].address(), Page::kPageSize);
  return Page::FromAddress(low);
}


Page* MemoryAllocator::FindLastPageInSameChunk(Page* p) {
  int chunk_id = GetChunkId(p);
  ASSERT(IsValidChunk(chunk_id));

  Address chunk_start = chunks_[chunk_id].address();
  size_t chunk_size = chunks_[chunk_id].size();

  Address high = RoundDown(chunk_start + chunk_size, Page::kPageSize);
  ASSERT(chunk_start <= p->address() && p->address() < high);

  return Page::FromAddress(high - Page::kPageSize);
}


#ifdef DEBUG
void MemoryAllocator::ReportStatistics() {
  float pct = static_cast<float>(capacity_ - size_) / capacity_;
  PrintF("  capacity: %" V8_PTR_PREFIX "d"
             ", used: %" V8_PTR_PREFIX "d"
             ", available: %%%d\n\n",
         capacity_, size_, static_cast<int>(pct*100));
}
#endif


void MemoryAllocator::RelinkPageListInChunkOrder(PagedSpace* space,
                                                 Page** first_page,
                                                 Page** last_page,
                                                 Page** last_page_in_use) {
  Page* first = NULL;
  Page* last = NULL;

  for (int i = 0, length = chunks_.length(); i < length; i++) {
    ChunkInfo& chunk = chunks_[i];

    if (chunk.owner() == space) {
      if (first == NULL) {
        Address low = RoundUp(chunk.address(), Page::kPageSize);
        first = Page::FromAddress(low);
      }
      last = RelinkPagesInChunk(i,
                                chunk.address(),
                                chunk.size(),
                                last,
                                last_page_in_use);
    }
  }

  if (first_page != NULL) {
    *first_page = first;
  }

  if (last_page != NULL) {
    *last_page = last;
  }
}


Page* MemoryAllocator::RelinkPagesInChunk(int chunk_id,
                                          Address chunk_start,
                                          size_t chunk_size,
                                          Page* prev,
                                          Page** last_page_in_use) {
  Address page_addr = RoundUp(chunk_start, Page::kPageSize);
  int pages_in_chunk = PagesInChunk(chunk_start, chunk_size);

  if (prev->is_valid()) {
    SetNextPage(prev, Page::FromAddress(page_addr));
  }

  for (int i = 0; i < pages_in_chunk; i++) {
    Page* p = Page::FromAddress(page_addr);
    p->opaque_header = OffsetFrom(page_addr + Page::kPageSize) | chunk_id;
    page_addr += Page::kPageSize;

    p->InvalidateWatermark(true);
    if (p->WasInUseBeforeMC()) {
      *last_page_in_use = p;
    }
  }

  // Set the next page of the last page to 0.
  Page* last_page = Page::FromAddress(page_addr - Page::kPageSize);
  last_page->opaque_header = OffsetFrom(0) | chunk_id;

  if (last_page->WasInUseBeforeMC()) {
    *last_page_in_use = last_page;
  }

  return last_page;
}


// -----------------------------------------------------------------------------
// PagedSpace implementation

PagedSpace::PagedSpace(Heap* heap,
                       intptr_t max_capacity,
                       AllocationSpace id,
                       Executability executable)
    : Space(heap, id, executable) {
  max_capacity_ = (RoundDown(max_capacity, Page::kPageSize) / Page::kPageSize)
                  * Page::kObjectAreaSize;
  accounting_stats_.Clear();

  allocation_info_.top = NULL;
  allocation_info_.limit = NULL;

  mc_forwarding_info_.top = NULL;
  mc_forwarding_info_.limit = NULL;
}


bool PagedSpace::Setup(Address start, size_t size) {
  if (HasBeenSetup()) return false;

  int num_pages = 0;
  // Try to use the virtual memory range passed to us.  If it is too small to
  // contain at least one page, ignore it and allocate instead.
  int pages_in_chunk = PagesInChunk(start, size);
  if (pages_in_chunk > 0) {
    first_page_ = Isolate::Current()->memory_allocator()->CommitPages(
        RoundUp(start, Page::kPageSize),
        Page::kPageSize * pages_in_chunk,
        this, &num_pages);
  } else {
    int requested_pages =
        Min(MemoryAllocator::kPagesPerChunk,
            static_cast<int>(max_capacity_ / Page::kObjectAreaSize));
    first_page_ =
        Isolate::Current()->memory_allocator()->AllocatePages(
            requested_pages, &num_pages, this);
    if (!first_page_->is_valid()) return false;
  }

  // We are sure that the first page is valid and that we have at least one
  // page.
  ASSERT(first_page_->is_valid());
  ASSERT(num_pages > 0);
  accounting_stats_.ExpandSpace(num_pages * Page::kObjectAreaSize);
  ASSERT(Capacity() <= max_capacity_);

  // Sequentially clear region marks in the newly allocated
  // pages and cache the current last page in the space.
  for (Page* p = first_page_; p->is_valid(); p = p->next_page()) {
    p->SetRegionMarks(Page::kAllRegionsCleanMarks);
    last_page_ = p;
  }

  // Use first_page_ for allocation.
  SetAllocationInfo(&allocation_info_, first_page_);

  page_list_is_chunk_ordered_ = true;

  return true;
}


bool PagedSpace::HasBeenSetup() {
  return (Capacity() > 0);
}


void PagedSpace::TearDown() {
  Isolate::Current()->memory_allocator()->FreeAllPages(this);
  first_page_ = NULL;
  accounting_stats_.Clear();
}


void PagedSpace::MarkAllPagesClean() {
  PageIterator it(this, PageIterator::ALL_PAGES);
  while (it.has_next()) {
    it.next()->SetRegionMarks(Page::kAllRegionsCleanMarks);
  }
}


MaybeObject* PagedSpace::FindObject(Address addr) {
  // Note: this function can only be called before or after mark-compact GC
  // because it accesses map pointers.
  ASSERT(!heap()->mark_compact_collector()->in_use());

  if (!Contains(addr)) return Failure::Exception();

  Page* p = Page::FromAddress(addr);
  ASSERT(IsUsed(p));
  Address cur = p->ObjectAreaStart();
  Address end = p->AllocationTop();
  while (cur < end) {
    HeapObject* obj = HeapObject::FromAddress(cur);
    Address next = cur + obj->Size();
    if ((cur <= addr) && (addr < next)) return obj;
    cur = next;
  }

  UNREACHABLE();
  return Failure::Exception();
}


bool PagedSpace::IsUsed(Page* page) {
  PageIterator it(this, PageIterator::PAGES_IN_USE);
  while (it.has_next()) {
    if (page == it.next()) return true;
  }
  return false;
}


void PagedSpace::SetAllocationInfo(AllocationInfo* alloc_info, Page* p) {
  alloc_info->top = p->ObjectAreaStart();
  alloc_info->limit = p->ObjectAreaEnd();
  ASSERT(alloc_info->VerifyPagedAllocation());
}


void PagedSpace::MCResetRelocationInfo() {
  // Set page indexes.
  int i = 0;
  PageIterator it(this, PageIterator::ALL_PAGES);
  while (it.has_next()) {
    Page* p = it.next();
    p->mc_page_index = i++;
  }

  // Set mc_forwarding_info_ to the first page in the space.
  SetAllocationInfo(&mc_forwarding_info_, first_page_);
  // All the bytes in the space are 'available'.  We will rediscover
  // allocated and wasted bytes during GC.
  accounting_stats_.Reset();
}


int PagedSpace::MCSpaceOffsetForAddress(Address addr) {
#ifdef DEBUG
  // The Contains function considers the address at the beginning of a
  // page in the page, MCSpaceOffsetForAddress considers it is in the
  // previous page.
  if (Page::IsAlignedToPageSize(addr)) {
    ASSERT(Contains(addr - kPointerSize));
  } else {
    ASSERT(Contains(addr));
  }
#endif

  // If addr is at the end of a page, it belongs to previous page
  Page* p = Page::IsAlignedToPageSize(addr)
            ? Page::FromAllocationTop(addr)
            : Page::FromAddress(addr);
  int index = p->mc_page_index;
  return (index * Page::kPageSize) + p->Offset(addr);
}


// Slow case for reallocating and promoting objects during a compacting
// collection.  This function is not space-specific.
HeapObject* PagedSpace::SlowMCAllocateRaw(int size_in_bytes) {
  Page* current_page = TopPageOf(mc_forwarding_info_);
  if (!current_page->next_page()->is_valid()) {
    if (!Expand(current_page)) {
      return NULL;
    }
  }

  // There are surely more pages in the space now.
  ASSERT(current_page->next_page()->is_valid());
  // We do not add the top of page block for current page to the space's
  // free list---the block may contain live objects so we cannot write
  // bookkeeping information to it.  Instead, we will recover top of page
  // blocks when we move objects to their new locations.
  //
  // We do however write the allocation pointer to the page.  The encoding
  // of forwarding addresses is as an offset in terms of live bytes, so we
  // need quick access to the allocation top of each page to decode
  // forwarding addresses.
  current_page->SetAllocationWatermark(mc_forwarding_info_.top);
  current_page->next_page()->InvalidateWatermark(true);
  SetAllocationInfo(&mc_forwarding_info_, current_page->next_page());
  return AllocateLinearly(&mc_forwarding_info_, size_in_bytes);
}


bool PagedSpace::Expand(Page* last_page) {
  ASSERT(max_capacity_ % Page::kObjectAreaSize == 0);
  ASSERT(Capacity() % Page::kObjectAreaSize == 0);

  if (Capacity() == max_capacity_) return false;

  ASSERT(Capacity() < max_capacity_);
  // Last page must be valid and its next page is invalid.
  ASSERT(last_page->is_valid() && !last_page->next_page()->is_valid());

  int available_pages =
      static_cast<int>((max_capacity_ - Capacity()) / Page::kObjectAreaSize);
  // We don't want to have to handle small chunks near the end so if there are
  // not kPagesPerChunk pages available without exceeding the max capacity then
  // act as if memory has run out.
  if (available_pages < MemoryAllocator::kPagesPerChunk) return false;

  int desired_pages = Min(available_pages, MemoryAllocator::kPagesPerChunk);
  Page* p = heap()->isolate()->memory_allocator()->AllocatePages(
      desired_pages, &desired_pages, this);
  if (!p->is_valid()) return false;

  accounting_stats_.ExpandSpace(desired_pages * Page::kObjectAreaSize);
  ASSERT(Capacity() <= max_capacity_);

  heap()->isolate()->memory_allocator()->SetNextPage(last_page, p);

  // Sequentially clear region marks of new pages and and cache the
  // new last page in the space.
  while (p->is_valid()) {
    p->SetRegionMarks(Page::kAllRegionsCleanMarks);
    last_page_ = p;
    p = p->next_page();
  }

  return true;
}


#ifdef DEBUG
int PagedSpace::CountTotalPages() {
  int count = 0;
  for (Page* p = first_page_; p->is_valid(); p = p->next_page()) {
    count++;
  }
  return count;
}
#endif


void PagedSpace::Shrink() {
  if (!page_list_is_chunk_ordered_) {
    // We can't shrink space if pages is not chunk-ordered
    // (see comment for class MemoryAllocator for definition).
    return;
  }

  // Release half of free pages.
  Page* top_page = AllocationTopPage();
  ASSERT(top_page->is_valid());

  // Count the number of pages we would like to free.
  int pages_to_free = 0;
  for (Page* p = top_page->next_page(); p->is_valid(); p = p->next_page()) {
    pages_to_free++;
  }

  // Free pages after top_page.
  Page* p = heap()->isolate()->memory_allocator()->
      FreePages(top_page->next_page());
  heap()->isolate()->memory_allocator()->SetNextPage(top_page, p);

  // Find out how many pages we failed to free and update last_page_.
  // Please note pages can only be freed in whole chunks.
  last_page_ = top_page;
  for (Page* p = top_page->next_page(); p->is_valid(); p = p->next_page()) {
    pages_to_free--;
    last_page_ = p;
  }

  accounting_stats_.ShrinkSpace(pages_to_free * Page::kObjectAreaSize);
  ASSERT(Capacity() == CountTotalPages() * Page::kObjectAreaSize);
}


bool PagedSpace::EnsureCapacity(int capacity) {
  if (Capacity() >= capacity) return true;

  // Start from the allocation top and loop to the last page in the space.
  Page* last_page = AllocationTopPage();
  Page* next_page = last_page->next_page();
  while (next_page->is_valid()) {
    last_page = heap()->isolate()->memory_allocator()->
        FindLastPageInSameChunk(next_page);
    next_page = last_page->next_page();
  }

  // Expand the space until it has the required capacity or expansion fails.
  do {
    if (!Expand(last_page)) return false;
    ASSERT(last_page->next_page()->is_valid());
    last_page =
        heap()->isolate()->memory_allocator()->FindLastPageInSameChunk(
            last_page->next_page());
  } while (Capacity() < capacity);

  return true;
}


#ifdef DEBUG
void PagedSpace::Print() { }
#endif


#ifdef DEBUG
// We do not assume that the PageIterator works, because it depends on the
// invariants we are checking during verification.
void PagedSpace::Verify(ObjectVisitor* visitor) {
  // The allocation pointer should be valid, and it should be in a page in the
  // space.
  ASSERT(allocation_info_.VerifyPagedAllocation());
  Page* top_page = Page::FromAllocationTop(allocation_info_.top);
  ASSERT(heap()->isolate()->memory_allocator()->IsPageInSpace(top_page, this));

  // Loop over all the pages.
  bool above_allocation_top = false;
  Page* current_page = first_page_;
  while (current_page->is_valid()) {
    if (above_allocation_top) {
      // We don't care what's above the allocation top.
    } else {
      Address top = current_page->AllocationTop();
      if (current_page == top_page) {
        ASSERT(top == allocation_info_.top);
        // The next page will be above the allocation top.
        above_allocation_top = true;
      }

      // It should be packed with objects from the bottom to the top.
      Address current = current_page->ObjectAreaStart();
      while (current < top) {
        HeapObject* object = HeapObject::FromAddress(current);

        // The first word should be a map, and we expect all map pointers to
        // be in map space.
        Map* map = object->map();
        ASSERT(map->IsMap());
        ASSERT(heap()->map_space()->Contains(map));

        // Perform space-specific object verification.
        VerifyObject(object);

        // The object itself should look OK.
        object->Verify();

        // All the interior pointers should be contained in the heap and
        // have page regions covering intergenerational references should be
        // marked dirty.
        int size = object->Size();
        object->IterateBody(map->instance_type(), size, visitor);

        current += size;
      }

      // The allocation pointer should not be in the middle of an object.
      ASSERT(current == top);
    }

    current_page = current_page->next_page();
  }
}
#endif


// -----------------------------------------------------------------------------
// NewSpace implementation


bool NewSpace::Setup(Address start, int size) {
  // Setup new space based on the preallocated memory block defined by
  // start and size. The provided space is divided into two semi-spaces.
  // To support fast containment testing in the new space, the size of
  // this chunk must be a power of two and it must be aligned to its size.
  int initial_semispace_capacity = heap()->InitialSemiSpaceSize();
  int maximum_semispace_capacity = heap()->MaxSemiSpaceSize();

  ASSERT(initial_semispace_capacity <= maximum_semispace_capacity);
  ASSERT(IsPowerOf2(maximum_semispace_capacity));

  // Allocate and setup the histogram arrays if necessary.
  allocated_histogram_ = NewArray<HistogramInfo>(LAST_TYPE + 1);
  promoted_histogram_ = NewArray<HistogramInfo>(LAST_TYPE + 1);

#define SET_NAME(name) allocated_histogram_[name].set_name(#name); \
                       promoted_histogram_[name].set_name(#name);
  INSTANCE_TYPE_LIST(SET_NAME)
#undef SET_NAME

  ASSERT(size == 2 * heap()->ReservedSemiSpaceSize());
  ASSERT(IsAddressAligned(start, size, 0));

  if (!to_space_.Setup(start,
                       initial_semispace_capacity,
                       maximum_semispace_capacity)) {
    return false;
  }
  if (!from_space_.Setup(start + maximum_semispace_capacity,
                         initial_semispace_capacity,
                         maximum_semispace_capacity)) {
    return false;
  }

  start_ = start;
  address_mask_ = ~(size - 1);
  object_mask_ = address_mask_ | kHeapObjectTagMask;
  object_expected_ = reinterpret_cast<uintptr_t>(start) | kHeapObjectTag;

  allocation_info_.top = to_space_.low();
  allocation_info_.limit = to_space_.high();
  mc_forwarding_info_.top = NULL;
  mc_forwarding_info_.limit = NULL;

  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
  return true;
}


void NewSpace::TearDown() {
  if (allocated_histogram_) {
    DeleteArray(allocated_histogram_);
    allocated_histogram_ = NULL;
  }
  if (promoted_histogram_) {
    DeleteArray(promoted_histogram_);
    promoted_histogram_ = NULL;
  }

  start_ = NULL;
  allocation_info_.top = NULL;
  allocation_info_.limit = NULL;
  mc_forwarding_info_.top = NULL;
  mc_forwarding_info_.limit = NULL;

  to_space_.TearDown();
  from_space_.TearDown();
}


void NewSpace::Flip() {
  SemiSpace tmp = from_space_;
  from_space_ = to_space_;
  to_space_ = tmp;
}


void NewSpace::Grow() {
  ASSERT(Capacity() < MaximumCapacity());
  if (to_space_.Grow()) {
    // Only grow from space if we managed to grow to space.
    if (!from_space_.Grow()) {
      // If we managed to grow to space but couldn't grow from space,
      // attempt to shrink to space.
      if (!to_space_.ShrinkTo(from_space_.Capacity())) {
        // We are in an inconsistent state because we could not
        // commit/uncommit memory from new space.
        V8::FatalProcessOutOfMemory("Failed to grow new space.");
      }
    }
  }
  allocation_info_.limit = to_space_.high();
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


void NewSpace::Shrink() {
  int new_capacity = Max(InitialCapacity(), 2 * SizeAsInt());
  int rounded_new_capacity =
      RoundUp(new_capacity, static_cast<int>(OS::AllocateAlignment()));
  if (rounded_new_capacity < Capacity() &&
      to_space_.ShrinkTo(rounded_new_capacity))  {
    // Only shrink from space if we managed to shrink to space.
    if (!from_space_.ShrinkTo(rounded_new_capacity)) {
      // If we managed to shrink to space but couldn't shrink from
      // space, attempt to grow to space again.
      if (!to_space_.GrowTo(from_space_.Capacity())) {
        // We are in an inconsistent state because we could not
        // commit/uncommit memory from new space.
        V8::FatalProcessOutOfMemory("Failed to shrink new space.");
      }
    }
  }
  allocation_info_.limit = to_space_.high();
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


void NewSpace::ResetAllocationInfo() {
  allocation_info_.top = to_space_.low();
  allocation_info_.limit = to_space_.high();
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


void NewSpace::MCResetRelocationInfo() {
  mc_forwarding_info_.top = from_space_.low();
  mc_forwarding_info_.limit = from_space_.high();
  ASSERT_SEMISPACE_ALLOCATION_INFO(mc_forwarding_info_, from_space_);
}


void NewSpace::MCCommitRelocationInfo() {
  // Assumes that the spaces have been flipped so that mc_forwarding_info_ is
  // valid allocation info for the to space.
  allocation_info_.top = mc_forwarding_info_.top;
  allocation_info_.limit = to_space_.high();
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
}


#ifdef DEBUG
// We do not use the SemispaceIterator because verification doesn't assume
// that it works (it depends on the invariants we are checking).
void NewSpace::Verify() {
  // The allocation pointer should be in the space or at the very end.
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);

  // There should be objects packed in from the low address up to the
  // allocation pointer.
  Address current = to_space_.low();
  while (current < top()) {
    HeapObject* object = HeapObject::FromAddress(current);

    // The first word should be a map, and we expect all map pointers to
    // be in map space.
    Map* map = object->map();
    ASSERT(map->IsMap());
    ASSERT(heap()->map_space()->Contains(map));

    // The object should not be code or a map.
    ASSERT(!object->IsMap());
    ASSERT(!object->IsCode());

    // The object itself should look OK.
    object->Verify();

    // All the interior pointers should be contained in the heap.
    VerifyPointersVisitor visitor;
    int size = object->Size();
    object->IterateBody(map->instance_type(), size, &visitor);

    current += size;
  }

  // The allocation pointer should not be in the middle of an object.
  ASSERT(current == top());
}
#endif


bool SemiSpace::Commit() {
  ASSERT(!is_committed());
  if (!heap()->isolate()->memory_allocator()->CommitBlock(
      start_, capacity_, executable())) {
    return false;
  }
  committed_ = true;
  return true;
}


bool SemiSpace::Uncommit() {
  ASSERT(is_committed());
  if (!heap()->isolate()->memory_allocator()->UncommitBlock(
      start_, capacity_)) {
    return false;
  }
  committed_ = false;
  return true;
}


// -----------------------------------------------------------------------------
// SemiSpace implementation

bool SemiSpace::Setup(Address start,
                      int initial_capacity,
                      int maximum_capacity) {
  // Creates a space in the young generation. The constructor does not
  // allocate memory from the OS.  A SemiSpace is given a contiguous chunk of
  // memory of size 'capacity' when set up, and does not grow or shrink
  // otherwise.  In the mark-compact collector, the memory region of the from
  // space is used as the marking stack. It requires contiguous memory
  // addresses.
  initial_capacity_ = initial_capacity;
  capacity_ = initial_capacity;
  maximum_capacity_ = maximum_capacity;
  committed_ = false;

  start_ = start;
  address_mask_ = ~(maximum_capacity - 1);
  object_mask_ = address_mask_ | kHeapObjectTagMask;
  object_expected_ = reinterpret_cast<uintptr_t>(start) | kHeapObjectTag;
  age_mark_ = start_;

  return Commit();
}


void SemiSpace::TearDown() {
  start_ = NULL;
  capacity_ = 0;
}


bool SemiSpace::Grow() {
  // Double the semispace size but only up to maximum capacity.
  int maximum_extra = maximum_capacity_ - capacity_;
  int extra = Min(RoundUp(capacity_, static_cast<int>(OS::AllocateAlignment())),
                  maximum_extra);
  if (!heap()->isolate()->memory_allocator()->CommitBlock(
      high(), extra, executable())) {
    return false;
  }
  capacity_ += extra;
  return true;
}


bool SemiSpace::GrowTo(int new_capacity) {
  ASSERT(new_capacity <= maximum_capacity_);
  ASSERT(new_capacity > capacity_);
  size_t delta = new_capacity - capacity_;
  ASSERT(IsAligned(delta, OS::AllocateAlignment()));
  if (!heap()->isolate()->memory_allocator()->CommitBlock(
      high(), delta, executable())) {
    return false;
  }
  capacity_ = new_capacity;
  return true;
}


bool SemiSpace::ShrinkTo(int new_capacity) {
  ASSERT(new_capacity >= initial_capacity_);
  ASSERT(new_capacity < capacity_);
  size_t delta = capacity_ - new_capacity;
  ASSERT(IsAligned(delta, OS::AllocateAlignment()));
  if (!heap()->isolate()->memory_allocator()->UncommitBlock(
      high() - delta, delta)) {
    return false;
  }
  capacity_ = new_capacity;
  return true;
}


#ifdef DEBUG
void SemiSpace::Print() { }


void SemiSpace::Verify() { }
#endif


// -----------------------------------------------------------------------------
// SemiSpaceIterator implementation.
SemiSpaceIterator::SemiSpaceIterator(NewSpace* space) {
  Initialize(space, space->bottom(), space->top(), NULL);
}


SemiSpaceIterator::SemiSpaceIterator(NewSpace* space,
                                     HeapObjectCallback size_func) {
  Initialize(space, space->bottom(), space->top(), size_func);
}


SemiSpaceIterator::SemiSpaceIterator(NewSpace* space, Address start) {
  Initialize(space, start, space->top(), NULL);
}


void SemiSpaceIterator::Initialize(NewSpace* space, Address start,
                                   Address end,
                                   HeapObjectCallback size_func) {
  ASSERT(space->ToSpaceContains(start));
  ASSERT(space->ToSpaceLow() <= end
         && end <= space->ToSpaceHigh());
  space_ = &space->to_space_;
  current_ = start;
  limit_ = end;
  size_func_ = size_func;
}


#ifdef DEBUG
// heap_histograms is shared, always clear it before using it.
static void ClearHistograms() {
  Isolate* isolate = Isolate::Current();
  // We reset the name each time, though it hasn't changed.
#define DEF_TYPE_NAME(name) isolate->heap_histograms()[name].set_name(#name);
  INSTANCE_TYPE_LIST(DEF_TYPE_NAME)
#undef DEF_TYPE_NAME

#define CLEAR_HISTOGRAM(name) isolate->heap_histograms()[name].clear();
  INSTANCE_TYPE_LIST(CLEAR_HISTOGRAM)
#undef CLEAR_HISTOGRAM

  isolate->js_spill_information()->Clear();
}


static void ClearCodeKindStatistics() {
  Isolate* isolate = Isolate::Current();
  for (int i = 0; i < Code::NUMBER_OF_KINDS; i++) {
    isolate->code_kind_statistics()[i] = 0;
  }
}


static void ReportCodeKindStatistics() {
  Isolate* isolate = Isolate::Current();
  const char* table[Code::NUMBER_OF_KINDS] = { NULL };

#define CASE(name)                            \
  case Code::name: table[Code::name] = #name; \
  break

  for (int i = 0; i < Code::NUMBER_OF_KINDS; i++) {
    switch (static_cast<Code::Kind>(i)) {
      CASE(FUNCTION);
      CASE(OPTIMIZED_FUNCTION);
      CASE(STUB);
      CASE(BUILTIN);
      CASE(LOAD_IC);
      CASE(KEYED_LOAD_IC);
      CASE(STORE_IC);
      CASE(KEYED_STORE_IC);
      CASE(CALL_IC);
      CASE(KEYED_CALL_IC);
      CASE(UNARY_OP_IC);
      CASE(BINARY_OP_IC);
      CASE(COMPARE_IC);
      CASE(TO_BOOLEAN_IC);
    }
  }

#undef CASE

  PrintF("\n   Code kind histograms: \n");
  for (int i = 0; i < Code::NUMBER_OF_KINDS; i++) {
    if (isolate->code_kind_statistics()[i] > 0) {
      PrintF("     %-20s: %10d bytes\n", table[i],
          isolate->code_kind_statistics()[i]);
    }
  }
  PrintF("\n");
}


static int CollectHistogramInfo(HeapObject* obj) {
  Isolate* isolate = Isolate::Current();
  InstanceType type = obj->map()->instance_type();
  ASSERT(0 <= type && type <= LAST_TYPE);
  ASSERT(isolate->heap_histograms()[type].name() != NULL);
  isolate->heap_histograms()[type].increment_number(1);
  isolate->heap_histograms()[type].increment_bytes(obj->Size());

  if (FLAG_collect_heap_spill_statistics && obj->IsJSObject()) {
    JSObject::cast(obj)->IncrementSpillStatistics(
        isolate->js_spill_information());
  }

  return obj->Size();
}


static void ReportHistogram(bool print_spill) {
  Isolate* isolate = Isolate::Current();
  PrintF("\n  Object Histogram:\n");
  for (int i = 0; i <= LAST_TYPE; i++) {
    if (isolate->heap_histograms()[i].number() > 0) {
      PrintF("    %-34s%10d (%10d bytes)\n",
             isolate->heap_histograms()[i].name(),
             isolate->heap_histograms()[i].number(),
             isolate->heap_histograms()[i].bytes());
    }
  }
  PrintF("\n");

  // Summarize string types.
  int string_number = 0;
  int string_bytes = 0;
#define INCREMENT(type, size, name, camel_name)      \
    string_number += isolate->heap_histograms()[type].number(); \
    string_bytes += isolate->heap_histograms()[type].bytes();
  STRING_TYPE_LIST(INCREMENT)
#undef INCREMENT
  if (string_number > 0) {
    PrintF("    %-34s%10d (%10d bytes)\n\n", "STRING_TYPE", string_number,
           string_bytes);
  }

  if (FLAG_collect_heap_spill_statistics && print_spill) {
    isolate->js_spill_information()->Print();
  }
}
#endif  // DEBUG


// Support for statistics gathering for --heap-stats and --log-gc.
void NewSpace::ClearHistograms() {
  for (int i = 0; i <= LAST_TYPE; i++) {
    allocated_histogram_[i].clear();
    promoted_histogram_[i].clear();
  }
}

// Because the copying collector does not touch garbage objects, we iterate
// the new space before a collection to get a histogram of allocated objects.
// This only happens when --log-gc flag is set.
void NewSpace::CollectStatistics() {
  ClearHistograms();
  SemiSpaceIterator it(this);
  for (HeapObject* obj = it.next(); obj != NULL; obj = it.next())
    RecordAllocation(obj);
}


static void DoReportStatistics(Isolate* isolate,
                               HistogramInfo* info, const char* description) {
  LOG(isolate, HeapSampleBeginEvent("NewSpace", description));
  // Lump all the string types together.
  int string_number = 0;
  int string_bytes = 0;
#define INCREMENT(type, size, name, camel_name)       \
    string_number += info[type].number();             \
    string_bytes += info[type].bytes();
  STRING_TYPE_LIST(INCREMENT)
#undef INCREMENT
  if (string_number > 0) {
    LOG(isolate,
        HeapSampleItemEvent("STRING_TYPE", string_number, string_bytes));
  }

  // Then do the other types.
  for (int i = FIRST_NONSTRING_TYPE; i <= LAST_TYPE; ++i) {
    if (info[i].number() > 0) {
      LOG(isolate,
          HeapSampleItemEvent(info[i].name(), info[i].number(),
                              info[i].bytes()));
    }
  }
  LOG(isolate, HeapSampleEndEvent("NewSpace", description));
}


void NewSpace::ReportStatistics() {
#ifdef DEBUG
  if (FLAG_heap_stats) {
    float pct = static_cast<float>(Available()) / Capacity();
    PrintF("  capacity: %" V8_PTR_PREFIX "d"
               ", available: %" V8_PTR_PREFIX "d, %%%d\n",
           Capacity(), Available(), static_cast<int>(pct*100));
    PrintF("\n  Object Histogram:\n");
    for (int i = 0; i <= LAST_TYPE; i++) {
      if (allocated_histogram_[i].number() > 0) {
        PrintF("    %-34s%10d (%10d bytes)\n",
               allocated_histogram_[i].name(),
               allocated_histogram_[i].number(),
               allocated_histogram_[i].bytes());
      }
    }
    PrintF("\n");
  }
#endif  // DEBUG

  if (FLAG_log_gc) {
    Isolate* isolate = ISOLATE;
    DoReportStatistics(isolate, allocated_histogram_, "allocated");
    DoReportStatistics(isolate, promoted_histogram_, "promoted");
  }
}


void NewSpace::RecordAllocation(HeapObject* obj) {
  InstanceType type = obj->map()->instance_type();
  ASSERT(0 <= type && type <= LAST_TYPE);
  allocated_histogram_[type].increment_number(1);
  allocated_histogram_[type].increment_bytes(obj->Size());
}


void NewSpace::RecordPromotion(HeapObject* obj) {
  InstanceType type = obj->map()->instance_type();
  ASSERT(0 <= type && type <= LAST_TYPE);
  promoted_histogram_[type].increment_number(1);
  promoted_histogram_[type].increment_bytes(obj->Size());
}


// -----------------------------------------------------------------------------
// Free lists for old object spaces implementation

void FreeListNode::set_size(Heap* heap, int size_in_bytes) {
  ASSERT(size_in_bytes > 0);
  ASSERT(IsAligned(size_in_bytes, kPointerSize));

  // We write a map and possibly size information to the block.  If the block
  // is big enough to be a ByteArray with at least one extra word (the next
  // pointer), we set its map to be the byte array map and its size to an
  // appropriate array length for the desired size from HeapObject::Size().
  // If the block is too small (eg, one or two words), to hold both a size
  // field and a next pointer, we give it a filler map that gives it the
  // correct size.
  if (size_in_bytes > ByteArray::kHeaderSize) {
    set_map(heap->raw_unchecked_byte_array_map());
    // Can't use ByteArray::cast because it fails during deserialization.
    ByteArray* this_as_byte_array = reinterpret_cast<ByteArray*>(this);
    this_as_byte_array->set_length(ByteArray::LengthFor(size_in_bytes));
  } else if (size_in_bytes == kPointerSize) {
    set_map(heap->raw_unchecked_one_pointer_filler_map());
  } else if (size_in_bytes == 2 * kPointerSize) {
    set_map(heap->raw_unchecked_two_pointer_filler_map());
  } else {
    UNREACHABLE();
  }
  // We would like to ASSERT(Size() == size_in_bytes) but this would fail during
  // deserialization because the byte array map is not done yet.
}


Address FreeListNode::next(Heap* heap) {
  ASSERT(IsFreeListNode(this));
  if (map() == heap->raw_unchecked_byte_array_map()) {
    ASSERT(Size() >= kNextOffset + kPointerSize);
    return Memory::Address_at(address() + kNextOffset);
  } else {
    return Memory::Address_at(address() + kPointerSize);
  }
}


void FreeListNode::set_next(Heap* heap, Address next) {
  ASSERT(IsFreeListNode(this));
  if (map() == heap->raw_unchecked_byte_array_map()) {
    ASSERT(Size() >= kNextOffset + kPointerSize);
    Memory::Address_at(address() + kNextOffset) = next;
  } else {
    Memory::Address_at(address() + kPointerSize) = next;
  }
}


OldSpaceFreeList::OldSpaceFreeList(Heap* heap, AllocationSpace owner)
  : heap_(heap),
    owner_(owner) {
  Reset();
}


void OldSpaceFreeList::Reset() {
  available_ = 0;
  for (int i = 0; i < kFreeListsLength; i++) {
    free_[i].head_node_ = NULL;
  }
  needs_rebuild_ = false;
  finger_ = kHead;
  free_[kHead].next_size_ = kEnd;
}


void OldSpaceFreeList::RebuildSizeList() {
  ASSERT(needs_rebuild_);
  int cur = kHead;
  for (int i = cur + 1; i < kFreeListsLength; i++) {
    if (free_[i].head_node_ != NULL) {
      free_[cur].next_size_ = i;
      cur = i;
    }
  }
  free_[cur].next_size_ = kEnd;
  needs_rebuild_ = false;
}


int OldSpaceFreeList::Free(Address start, int size_in_bytes) {
#ifdef DEBUG
  Isolate::Current()->memory_allocator()->ZapBlock(start, size_in_bytes);
#endif
  FreeListNode* node = FreeListNode::FromAddress(start);
  node->set_size(heap_, size_in_bytes);

  // We don't use the freelists in compacting mode.  This makes it more like a
  // GC that only has mark-sweep-compact and doesn't have a mark-sweep
  // collector.
  if (FLAG_always_compact) {
    return size_in_bytes;
  }

  // Early return to drop too-small blocks on the floor (one or two word
  // blocks cannot hold a map pointer, a size field, and a pointer to the
  // next block in the free list).
  if (size_in_bytes < kMinBlockSize) {
    return size_in_bytes;
  }

  // Insert other blocks at the head of an exact free list.
  int index = size_in_bytes >> kPointerSizeLog2;
  node->set_next(heap_, free_[index].head_node_);
  free_[index].head_node_ = node->address();
  available_ += size_in_bytes;
  needs_rebuild_ = true;
  return 0;
}


MaybeObject* OldSpaceFreeList::Allocate(int size_in_bytes, int* wasted_bytes) {
  ASSERT(0 < size_in_bytes);
  ASSERT(size_in_bytes <= kMaxBlockSize);
  ASSERT(IsAligned(size_in_bytes, kPointerSize));

  if (needs_rebuild_) RebuildSizeList();
  int index = size_in_bytes >> kPointerSizeLog2;
  // Check for a perfect fit.
  if (free_[index].head_node_ != NULL) {
    FreeListNode* node = FreeListNode::FromAddress(free_[index].head_node_);
    // If this was the last block of its size, remove the size.
    if ((free_[index].head_node_ = node->next(heap_)) == NULL)
      RemoveSize(index);
    available_ -= size_in_bytes;
    *wasted_bytes = 0;
    ASSERT(!FLAG_always_compact);  // We only use the freelists with mark-sweep.
    return node;
  }
  // Search the size list for the best fit.
  int prev = finger_ < index ? finger_ : kHead;
  int cur = FindSize(index, &prev);
  ASSERT(index < cur);
  if (cur == kEnd) {
    // No large enough size in list.
    *wasted_bytes = 0;
    return Failure::RetryAfterGC(owner_);
  }
  ASSERT(!FLAG_always_compact);  // We only use the freelists with mark-sweep.
  int rem = cur - index;
  int rem_bytes = rem << kPointerSizeLog2;
  FreeListNode* cur_node = FreeListNode::FromAddress(free_[cur].head_node_);
  ASSERT(cur_node->Size() == (cur << kPointerSizeLog2));
  FreeListNode* rem_node = FreeListNode::FromAddress(free_[cur].head_node_ +
                                                     size_in_bytes);
  // Distinguish the cases prev < rem < cur and rem <= prev < cur
  // to avoid many redundant tests and calls to Insert/RemoveSize.
  if (prev < rem) {
    // Simple case: insert rem between prev and cur.
    finger_ = prev;
    free_[prev].next_size_ = rem;
    // If this was the last block of size cur, remove the size.
    if ((free_[cur].head_node_ = cur_node->next(heap_)) == NULL) {
      free_[rem].next_size_ = free_[cur].next_size_;
    } else {
      free_[rem].next_size_ = cur;
    }
    // Add the remainder block.
    rem_node->set_size(heap_, rem_bytes);
    rem_node->set_next(heap_, free_[rem].head_node_);
    free_[rem].head_node_ = rem_node->address();
  } else {
    // If this was the last block of size cur, remove the size.
    if ((free_[cur].head_node_ = cur_node->next(heap_)) == NULL) {
      finger_ = prev;
      free_[prev].next_size_ = free_[cur].next_size_;
    }
    if (rem_bytes < kMinBlockSize) {
      // Too-small remainder is wasted.
      rem_node->set_size(heap_, rem_bytes);
      available_ -= size_in_bytes + rem_bytes;
      *wasted_bytes = rem_bytes;
      return cur_node;
    }
    // Add the remainder block and, if needed, insert its size.
    rem_node->set_size(heap_, rem_bytes);
    rem_node->set_next(heap_, free_[rem].head_node_);
    free_[rem].head_node_ = rem_node->address();
    if (rem_node->next(heap_) == NULL) InsertSize(rem);
  }
  available_ -= size_in_bytes;
  *wasted_bytes = 0;
  return cur_node;
}


void OldSpaceFreeList::MarkNodes() {
  for (int i = 0; i < kFreeListsLength; i++) {
    Address cur_addr = free_[i].head_node_;
    while (cur_addr != NULL) {
      FreeListNode* cur_node = FreeListNode::FromAddress(cur_addr);
      cur_addr = cur_node->next(heap_);
      cur_node->SetMark();
    }
  }
}


#ifdef DEBUG
bool OldSpaceFreeList::Contains(FreeListNode* node) {
  for (int i = 0; i < kFreeListsLength; i++) {
    Address cur_addr = free_[i].head_node_;
    while (cur_addr != NULL) {
      FreeListNode* cur_node = FreeListNode::FromAddress(cur_addr);
      if (cur_node == node) return true;
      cur_addr = cur_node->next(heap_);
    }
  }
  return false;
}
#endif


FixedSizeFreeList::FixedSizeFreeList(Heap* heap,
                                     AllocationSpace owner,
                                     int object_size)
    : heap_(heap), owner_(owner), object_size_(object_size) {
  Reset();
}


void FixedSizeFreeList::Reset() {
  available_ = 0;
  head_ = tail_ = NULL;
}


void FixedSizeFreeList::Free(Address start) {
#ifdef DEBUG
  Isolate::Current()->memory_allocator()->ZapBlock(start, object_size_);
#endif
  // We only use the freelists with mark-sweep.
  ASSERT(!HEAP->mark_compact_collector()->IsCompacting());
  FreeListNode* node = FreeListNode::FromAddress(start);
  node->set_size(heap_, object_size_);
  node->set_next(heap_, NULL);
  if (head_ == NULL) {
    tail_ = head_ = node->address();
  } else {
    FreeListNode::FromAddress(tail_)->set_next(heap_, node->address());
    tail_ = node->address();
  }
  available_ += object_size_;
}


MaybeObject* FixedSizeFreeList::Allocate() {
  if (head_ == NULL) {
    return Failure::RetryAfterGC(owner_);
  }

  ASSERT(!FLAG_always_compact);  // We only use the freelists with mark-sweep.
  FreeListNode* node = FreeListNode::FromAddress(head_);
  head_ = node->next(heap_);
  available_ -= object_size_;
  return node;
}


void FixedSizeFreeList::MarkNodes() {
  Address cur_addr = head_;
  while (cur_addr != NULL && cur_addr != tail_) {
    FreeListNode* cur_node = FreeListNode::FromAddress(cur_addr);
    cur_addr = cur_node->next(heap_);
    cur_node->SetMark();
  }
}


// -----------------------------------------------------------------------------
// OldSpace implementation

void OldSpace::PrepareForMarkCompact(bool will_compact) {
  // Call prepare of the super class.
  PagedSpace::PrepareForMarkCompact(will_compact);

  if (will_compact) {
    // Reset relocation info.  During a compacting collection, everything in
    // the space is considered 'available' and we will rediscover live data
    // and waste during the collection.
    MCResetRelocationInfo();
    ASSERT(Available() == Capacity());
  } else {
    // During a non-compacting collection, everything below the linear
    // allocation pointer is considered allocated (everything above is
    // available) and we will rediscover available and wasted bytes during
    // the collection.
    accounting_stats_.AllocateBytes(free_list_.available());
    accounting_stats_.FillWastedBytes(Waste());
  }

  // Clear the free list before a full GC---it will be rebuilt afterward.
  free_list_.Reset();
}


void OldSpace::MCCommitRelocationInfo() {
  // Update fast allocation info.
  allocation_info_.top = mc_forwarding_info_.top;
  allocation_info_.limit = mc_forwarding_info_.limit;
  ASSERT(allocation_info_.VerifyPagedAllocation());

  // The space is compacted and we haven't yet built free lists or
  // wasted any space.
  ASSERT(Waste() == 0);
  ASSERT(AvailableFree() == 0);

  // Build the free list for the space.
  int computed_size = 0;
  PageIterator it(this, PageIterator::PAGES_USED_BY_MC);
  while (it.has_next()) {
    Page* p = it.next();
    // Space below the relocation pointer is allocated.
    computed_size +=
        static_cast<int>(p->AllocationWatermark() - p->ObjectAreaStart());
    if (it.has_next()) {
      // Free the space at the top of the page.
      int extra_size =
          static_cast<int>(p->ObjectAreaEnd() - p->AllocationWatermark());
      if (extra_size > 0) {
        int wasted_bytes = free_list_.Free(p->AllocationWatermark(),
                                           extra_size);
        // The bytes we have just "freed" to add to the free list were
        // already accounted as available.
        accounting_stats_.WasteBytes(wasted_bytes);
      }
    }
  }

  // Make sure the computed size - based on the used portion of the pages in
  // use - matches the size obtained while computing forwarding addresses.
  ASSERT(computed_size == Size());
}


bool NewSpace::ReserveSpace(int bytes) {
  // We can't reliably unpack a partial snapshot that needs more new space
  // space than the minimum NewSpace size.
  ASSERT(bytes <= InitialCapacity());
  Address limit = allocation_info_.limit;
  Address top = allocation_info_.top;
  return limit - top >= bytes;
}


void PagedSpace::FreePages(Page* prev, Page* last) {
  if (last == AllocationTopPage()) {
    // Pages are already at the end of used pages.
    return;
  }

  Page* first = NULL;

  // Remove pages from the list.
  if (prev == NULL) {
    first = first_page_;
    first_page_ = last->next_page();
  } else {
    first = prev->next_page();
    heap()->isolate()->memory_allocator()->SetNextPage(
        prev, last->next_page());
  }

  // Attach it after the last page.
  heap()->isolate()->memory_allocator()->SetNextPage(last_page_, first);
  last_page_ = last;
  heap()->isolate()->memory_allocator()->SetNextPage(last, NULL);

  // Clean them up.
  do {
    first->InvalidateWatermark(true);
    first->SetAllocationWatermark(first->ObjectAreaStart());
    first->SetCachedAllocationWatermark(first->ObjectAreaStart());
    first->SetRegionMarks(Page::kAllRegionsCleanMarks);
    first = first->next_page();
  } while (first != NULL);

  // Order of pages in this space might no longer be consistent with
  // order of pages in chunks.
  page_list_is_chunk_ordered_ = false;
}


void PagedSpace::RelinkPageListInChunkOrder(bool deallocate_blocks) {
  const bool add_to_freelist = true;

  // Mark used and unused pages to properly fill unused pages
  // after reordering.
  PageIterator all_pages_iterator(this, PageIterator::ALL_PAGES);
  Page* last_in_use = AllocationTopPage();
  bool in_use = true;

  while (all_pages_iterator.has_next()) {
    Page* p = all_pages_iterator.next();
    p->SetWasInUseBeforeMC(in_use);
    if (p == last_in_use) {
      // We passed a page containing allocation top. All consequent
      // pages are not used.
      in_use = false;
    }
  }

  if (page_list_is_chunk_ordered_) return;

  Page* new_last_in_use = Page::FromAddress(NULL);
  heap()->isolate()->memory_allocator()->RelinkPageListInChunkOrder(
      this, &first_page_, &last_page_, &new_last_in_use);
  ASSERT(new_last_in_use->is_valid());

  if (new_last_in_use != last_in_use) {
    // Current allocation top points to a page which is now in the middle
    // of page list. We should move allocation top forward to the new last
    // used page so various object iterators will continue to work properly.
    int size_in_bytes = static_cast<int>(PageAllocationLimit(last_in_use) -
                                         last_in_use->AllocationTop());

    last_in_use->SetAllocationWatermark(last_in_use->AllocationTop());
    if (size_in_bytes > 0) {
      Address start = last_in_use->AllocationTop();
      if (deallocate_blocks) {
        accounting_stats_.AllocateBytes(size_in_bytes);
        DeallocateBlock(start, size_in_bytes, add_to_freelist);
      } else {
        heap()->CreateFillerObjectAt(start, size_in_bytes);
      }
    }

    // New last in use page was in the middle of the list before
    // sorting so it full.
    SetTop(new_last_in_use->AllocationTop());

    ASSERT(AllocationTopPage() == new_last_in_use);
    ASSERT(AllocationTopPage()->WasInUseBeforeMC());
  }

  PageIterator pages_in_use_iterator(this, PageIterator::PAGES_IN_USE);
  while (pages_in_use_iterator.has_next()) {
    Page* p = pages_in_use_iterator.next();
    if (!p->WasInUseBeforeMC()) {
      // Empty page is in the middle of a sequence of used pages.
      // Allocate it as a whole and deallocate immediately.
      int size_in_bytes = static_cast<int>(PageAllocationLimit(p) -
                                           p->ObjectAreaStart());

      p->SetAllocationWatermark(p->ObjectAreaStart());
      Address start = p->ObjectAreaStart();
      if (deallocate_blocks) {
        accounting_stats_.AllocateBytes(size_in_bytes);
        DeallocateBlock(start, size_in_bytes, add_to_freelist);
      } else {
        heap()->CreateFillerObjectAt(start, size_in_bytes);
      }
    }
  }

  page_list_is_chunk_ordered_ = true;
}


void PagedSpace::PrepareForMarkCompact(bool will_compact) {
  if (will_compact) {
    RelinkPageListInChunkOrder(false);
  }
}


bool PagedSpace::ReserveSpace(int bytes) {
  Address limit = allocation_info_.limit;
  Address top = allocation_info_.top;
  if (limit - top >= bytes) return true;

  // There wasn't enough space in the current page.  Lets put the rest
  // of the page on the free list and start a fresh page.
  PutRestOfCurrentPageOnFreeList(TopPageOf(allocation_info_));

  Page* reserved_page = TopPageOf(allocation_info_);
  int bytes_left_to_reserve = bytes;
  while (bytes_left_to_reserve > 0) {
    if (!reserved_page->next_page()->is_valid()) {
      if (heap()->OldGenerationAllocationLimitReached()) return false;
      Expand(reserved_page);
    }
    bytes_left_to_reserve -= Page::kPageSize;
    reserved_page = reserved_page->next_page();
    if (!reserved_page->is_valid()) return false;
  }
  ASSERT(TopPageOf(allocation_info_)->next_page()->is_valid());
  TopPageOf(allocation_info_)->next_page()->InvalidateWatermark(true);
  SetAllocationInfo(&allocation_info_,
                    TopPageOf(allocation_info_)->next_page());
  return true;
}


// You have to call this last, since the implementation from PagedSpace
// doesn't know that memory was 'promised' to large object space.
bool LargeObjectSpace::ReserveSpace(int bytes) {
  return heap()->OldGenerationSpaceAvailable() >= bytes;
}


// Slow case for normal allocation.  Try in order: (1) allocate in the next
// page in the space, (2) allocate off the space's free list, (3) expand the
// space, (4) fail.
HeapObject* OldSpace::SlowAllocateRaw(int size_in_bytes) {
  // Linear allocation in this space has failed.  If there is another page
  // in the space, move to that page and allocate there.  This allocation
  // should succeed (size_in_bytes should not be greater than a page's
  // object area size).
  Page* current_page = TopPageOf(allocation_info_);
  if (current_page->next_page()->is_valid()) {
    return AllocateInNextPage(current_page, size_in_bytes);
  }

  // There is no next page in this space.  Try free list allocation unless that
  // is currently forbidden.
  if (!heap()->linear_allocation()) {
    int wasted_bytes;
    Object* result;
    MaybeObject* maybe = free_list_.Allocate(size_in_bytes, &wasted_bytes);
    accounting_stats_.WasteBytes(wasted_bytes);
    if (maybe->ToObject(&result)) {
      accounting_stats_.AllocateBytes(size_in_bytes);

      HeapObject* obj = HeapObject::cast(result);
      Page* p = Page::FromAddress(obj->address());

      if (obj->address() >= p->AllocationWatermark()) {
        // There should be no hole between the allocation watermark
        // and allocated object address.
        // Memory above the allocation watermark was not swept and
        // might contain garbage pointers to new space.
        ASSERT(obj->address() == p->AllocationWatermark());
        p->SetAllocationWatermark(obj->address() + size_in_bytes);
      }

      return obj;
    }
  }

  // Free list allocation failed and there is no next page.  Fail if we have
  // hit the old generation size limit that should cause a garbage
  // collection.
  if (!heap()->always_allocate() &&
      heap()->OldGenerationAllocationLimitReached()) {
    return NULL;
  }

  // Try to expand the space and allocate in the new next page.
  ASSERT(!current_page->next_page()->is_valid());
  if (Expand(current_page)) {
    return AllocateInNextPage(current_page, size_in_bytes);
  }

  // Finally, fail.
  return NULL;
}


void OldSpace::PutRestOfCurrentPageOnFreeList(Page* current_page) {
  current_page->SetAllocationWatermark(allocation_info_.top);
  int free_size =
      static_cast<int>(current_page->ObjectAreaEnd() - allocation_info_.top);
  if (free_size > 0) {
    int wasted_bytes = free_list_.Free(allocation_info_.top, free_size);
    accounting_stats_.WasteBytes(wasted_bytes);
  }
}


void FixedSpace::PutRestOfCurrentPageOnFreeList(Page* current_page) {
  current_page->SetAllocationWatermark(allocation_info_.top);
  int free_size =
      static_cast<int>(current_page->ObjectAreaEnd() - allocation_info_.top);
  // In the fixed space free list all the free list items have the right size.
  // We use up the rest of the page while preserving this invariant.
  while (free_size >= object_size_in_bytes_) {
    free_list_.Free(allocation_info_.top);
    allocation_info_.top += object_size_in_bytes_;
    free_size -= object_size_in_bytes_;
    accounting_stats_.WasteBytes(object_size_in_bytes_);
  }
}


// Add the block at the top of the page to the space's free list, set the
// allocation info to the next page (assumed to be one), and allocate
// linearly there.
HeapObject* OldSpace::AllocateInNextPage(Page* current_page,
                                         int size_in_bytes) {
  ASSERT(current_page->next_page()->is_valid());
  Page* next_page = current_page->next_page();
  next_page->ClearGCFields();
  PutRestOfCurrentPageOnFreeList(current_page);
  SetAllocationInfo(&allocation_info_, next_page);
  return AllocateLinearly(&allocation_info_, size_in_bytes);
}


void OldSpace::DeallocateBlock(Address start,
                                 int size_in_bytes,
                                 bool add_to_freelist) {
  Free(start, size_in_bytes, add_to_freelist);
}


#ifdef DEBUG
void PagedSpace::ReportCodeStatistics() {
  Isolate* isolate = Isolate::Current();
  CommentStatistic* comments_statistics =
      isolate->paged_space_comments_statistics();
  ReportCodeKindStatistics();
  PrintF("Code comment statistics (\"   [ comment-txt   :    size/   "
         "count  (average)\"):\n");
  for (int i = 0; i <= CommentStatistic::kMaxComments; i++) {
    const CommentStatistic& cs = comments_statistics[i];
    if (cs.size > 0) {
      PrintF("   %-30s: %10d/%6d     (%d)\n", cs.comment, cs.size, cs.count,
             cs.size/cs.count);
    }
  }
  PrintF("\n");
}


void PagedSpace::ResetCodeStatistics() {
  Isolate* isolate = Isolate::Current();
  CommentStatistic* comments_statistics =
      isolate->paged_space_comments_statistics();
  ClearCodeKindStatistics();
  for (int i = 0; i < CommentStatistic::kMaxComments; i++) {
    comments_statistics[i].Clear();
  }
  comments_statistics[CommentStatistic::kMaxComments].comment = "Unknown";
  comments_statistics[CommentStatistic::kMaxComments].size = 0;
  comments_statistics[CommentStatistic::kMaxComments].count = 0;
}


// Adds comment to 'comment_statistics' table. Performance OK as long as
// 'kMaxComments' is small
static void EnterComment(Isolate* isolate, const char* comment, int delta) {
  CommentStatistic* comments_statistics =
      isolate->paged_space_comments_statistics();
  // Do not count empty comments
  if (delta <= 0) return;
  CommentStatistic* cs = &comments_statistics[CommentStatistic::kMaxComments];
  // Search for a free or matching entry in 'comments_statistics': 'cs'
  // points to result.
  for (int i = 0; i < CommentStatistic::kMaxComments; i++) {
    if (comments_statistics[i].comment == NULL) {
      cs = &comments_statistics[i];
      cs->comment = comment;
      break;
    } else if (strcmp(comments_statistics[i].comment, comment) == 0) {
      cs = &comments_statistics[i];
      break;
    }
  }
  // Update entry for 'comment'
  cs->size += delta;
  cs->count += 1;
}


// Call for each nested comment start (start marked with '[ xxx', end marked
// with ']'.  RelocIterator 'it' must point to a comment reloc info.
static void CollectCommentStatistics(Isolate* isolate, RelocIterator* it) {
  ASSERT(!it->done());
  ASSERT(it->rinfo()->rmode() == RelocInfo::COMMENT);
  const char* tmp = reinterpret_cast<const char*>(it->rinfo()->data());
  if (tmp[0] != '[') {
    // Not a nested comment; skip
    return;
  }

  // Search for end of nested comment or a new nested comment
  const char* const comment_txt =
      reinterpret_cast<const char*>(it->rinfo()->data());
  const byte* prev_pc = it->rinfo()->pc();
  int flat_delta = 0;
  it->next();
  while (true) {
    // All nested comments must be terminated properly, and therefore exit
    // from loop.
    ASSERT(!it->done());
    if (it->rinfo()->rmode() == RelocInfo::COMMENT) {
      const char* const txt =
          reinterpret_cast<const char*>(it->rinfo()->data());
      flat_delta += static_cast<int>(it->rinfo()->pc() - prev_pc);
      if (txt[0] == ']') break;  // End of nested  comment
      // A new comment
      CollectCommentStatistics(isolate, it);
      // Skip code that was covered with previous comment
      prev_pc = it->rinfo()->pc();
    }
    it->next();
  }
  EnterComment(isolate, comment_txt, flat_delta);
}


// Collects code size statistics:
// - by code kind
// - by code comment
void PagedSpace::CollectCodeStatistics() {
  Isolate* isolate = heap()->isolate();
  HeapObjectIterator obj_it(this);
  for (HeapObject* obj = obj_it.next(); obj != NULL; obj = obj_it.next()) {
    if (obj->IsCode()) {
      Code* code = Code::cast(obj);
      isolate->code_kind_statistics()[code->kind()] += code->Size();
      RelocIterator it(code);
      int delta = 0;
      const byte* prev_pc = code->instruction_start();
      while (!it.done()) {
        if (it.rinfo()->rmode() == RelocInfo::COMMENT) {
          delta += static_cast<int>(it.rinfo()->pc() - prev_pc);
          CollectCommentStatistics(isolate, &it);
          prev_pc = it.rinfo()->pc();
        }
        it.next();
      }

      ASSERT(code->instruction_start() <= prev_pc &&
             prev_pc <= code->instruction_end());
      delta += static_cast<int>(code->instruction_end() - prev_pc);
      EnterComment(isolate, "NoComment", delta);
    }
  }
}


void OldSpace::ReportStatistics() {
  int pct = static_cast<int>(Available() * 100 / Capacity());
  PrintF("  capacity: %" V8_PTR_PREFIX "d"
             ", waste: %" V8_PTR_PREFIX "d"
             ", available: %" V8_PTR_PREFIX "d, %%%d\n",
         Capacity(), Waste(), Available(), pct);

  ClearHistograms();
  HeapObjectIterator obj_it(this);
  for (HeapObject* obj = obj_it.next(); obj != NULL; obj = obj_it.next())
    CollectHistogramInfo(obj);
  ReportHistogram(true);
}
#endif

// -----------------------------------------------------------------------------
// FixedSpace implementation

void FixedSpace::PrepareForMarkCompact(bool will_compact) {
  // Call prepare of the super class.
  PagedSpace::PrepareForMarkCompact(will_compact);

  if (will_compact) {
    // Reset relocation info.
    MCResetRelocationInfo();

    // During a compacting collection, everything in the space is considered
    // 'available' (set by the call to MCResetRelocationInfo) and we will
    // rediscover live and wasted bytes during the collection.
    ASSERT(Available() == Capacity());
  } else {
    // During a non-compacting collection, everything below the linear
    // allocation pointer except wasted top-of-page blocks is considered
    // allocated and we will rediscover available bytes during the
    // collection.
    accounting_stats_.AllocateBytes(free_list_.available());
  }

  // Clear the free list before a full GC---it will be rebuilt afterward.
  free_list_.Reset();
}


void FixedSpace::MCCommitRelocationInfo() {
  // Update fast allocation info.
  allocation_info_.top = mc_forwarding_info_.top;
  allocation_info_.limit = mc_forwarding_info_.limit;
  ASSERT(allocation_info_.VerifyPagedAllocation());

  // The space is compacted and we haven't yet wasted any space.
  ASSERT(Waste() == 0);

  // Update allocation_top of each page in use and compute waste.
  int computed_size = 0;
  PageIterator it(this, PageIterator::PAGES_USED_BY_MC);
  while (it.has_next()) {
    Page* page = it.next();
    Address page_top = page->AllocationTop();
    computed_size += static_cast<int>(page_top - page->ObjectAreaStart());
    if (it.has_next()) {
      accounting_stats_.WasteBytes(
          static_cast<int>(page->ObjectAreaEnd() - page_top));
      page->SetAllocationWatermark(page_top);
    }
  }

  // Make sure the computed size - based on the used portion of the
  // pages in use - matches the size we adjust during allocation.
  ASSERT(computed_size == Size());
}


// Slow case for normal allocation. Try in order: (1) allocate in the next
// page in the space, (2) allocate off the space's free list, (3) expand the
// space, (4) fail.
HeapObject* FixedSpace::SlowAllocateRaw(int size_in_bytes) {
  ASSERT_EQ(object_size_in_bytes_, size_in_bytes);
  // Linear allocation in this space has failed.  If there is another page
  // in the space, move to that page and allocate there.  This allocation
  // should succeed.
  Page* current_page = TopPageOf(allocation_info_);
  if (current_page->next_page()->is_valid()) {
    return AllocateInNextPage(current_page, size_in_bytes);
  }

  // There is no next page in this space.  Try free list allocation unless
  // that is currently forbidden.  The fixed space free list implicitly assumes
  // that all free blocks are of the fixed size.
  if (!heap()->linear_allocation()) {
    Object* result;
    MaybeObject* maybe = free_list_.Allocate();
    if (maybe->ToObject(&result)) {
      accounting_stats_.AllocateBytes(size_in_bytes);
      HeapObject* obj = HeapObject::cast(result);
      Page* p = Page::FromAddress(obj->address());

      if (obj->address() >= p->AllocationWatermark()) {
        // There should be no hole between the allocation watermark
        // and allocated object address.
        // Memory above the allocation watermark was not swept and
        // might contain garbage pointers to new space.
        ASSERT(obj->address() == p->AllocationWatermark());
        p->SetAllocationWatermark(obj->address() + size_in_bytes);
      }

      return obj;
    }
  }

  // Free list allocation failed and there is no next page.  Fail if we have
  // hit the old generation size limit that should cause a garbage
  // collection.
  if (!heap()->always_allocate() &&
      heap()->OldGenerationAllocationLimitReached()) {
    return NULL;
  }

  // Try to expand the space and allocate in the new next page.
  ASSERT(!current_page->next_page()->is_valid());
  if (Expand(current_page)) {
    return AllocateInNextPage(current_page, size_in_bytes);
  }

  // Finally, fail.
  return NULL;
}


// Move to the next page (there is assumed to be one) and allocate there.
// The top of page block is always wasted, because it is too small to hold a
// map.
HeapObject* FixedSpace::AllocateInNextPage(Page* current_page,
                                           int size_in_bytes) {
  ASSERT(current_page->next_page()->is_valid());
  ASSERT(allocation_info_.top == PageAllocationLimit(current_page));
  ASSERT_EQ(object_size_in_bytes_, size_in_bytes);
  Page* next_page = current_page->next_page();
  next_page->ClearGCFields();
  current_page->SetAllocationWatermark(allocation_info_.top);
  accounting_stats_.WasteBytes(page_extra_);
  SetAllocationInfo(&allocation_info_, next_page);
  return AllocateLinearly(&allocation_info_, size_in_bytes);
}


void FixedSpace::DeallocateBlock(Address start,
                                 int size_in_bytes,
                                 bool add_to_freelist) {
  // Free-list elements in fixed space are assumed to have a fixed size.
  // We break the free block into chunks and add them to the free list
  // individually.
  int size = object_size_in_bytes();
  ASSERT(size_in_bytes % size == 0);
  Address end = start + size_in_bytes;
  for (Address a = start; a < end; a += size) {
    Free(a, add_to_freelist);
  }
}


#ifdef DEBUG
void FixedSpace::ReportStatistics() {
  int pct = static_cast<int>(Available() * 100 / Capacity());
  PrintF("  capacity: %" V8_PTR_PREFIX "d"
             ", waste: %" V8_PTR_PREFIX "d"
             ", available: %" V8_PTR_PREFIX "d, %%%d\n",
         Capacity(), Waste(), Available(), pct);

  ClearHistograms();
  HeapObjectIterator obj_it(this);
  for (HeapObject* obj = obj_it.next(); obj != NULL; obj = obj_it.next())
    CollectHistogramInfo(obj);
  ReportHistogram(false);
}
#endif


// -----------------------------------------------------------------------------
// MapSpace implementation

void MapSpace::PrepareForMarkCompact(bool will_compact) {
  // Call prepare of the super class.
  FixedSpace::PrepareForMarkCompact(will_compact);

  if (will_compact) {
    // Initialize map index entry.
    int page_count = 0;
    PageIterator it(this, PageIterator::ALL_PAGES);
    while (it.has_next()) {
      ASSERT_MAP_PAGE_INDEX(page_count);

      Page* p = it.next();
      ASSERT(p->mc_page_index == page_count);

      page_addresses_[page_count++] = p->address();
    }
  }
}


#ifdef DEBUG
void MapSpace::VerifyObject(HeapObject* object) {
  // The object should be a map or a free-list node.
  ASSERT(object->IsMap() || object->IsByteArray());
}
#endif


// -----------------------------------------------------------------------------
// GlobalPropertyCellSpace implementation

#ifdef DEBUG
void CellSpace::VerifyObject(HeapObject* object) {
  // The object should be a global object property cell or a free-list node.
  ASSERT(object->IsJSGlobalPropertyCell() ||
         object->map() == heap()->two_pointer_filler_map());
}
#endif


// -----------------------------------------------------------------------------
// LargeObjectIterator

LargeObjectIterator::LargeObjectIterator(LargeObjectSpace* space) {
  current_ = space->first_chunk_;
  size_func_ = NULL;
}


LargeObjectIterator::LargeObjectIterator(LargeObjectSpace* space,
                                         HeapObjectCallback size_func) {
  current_ = space->first_chunk_;
  size_func_ = size_func;
}


HeapObject* LargeObjectIterator::next() {
  if (current_ == NULL) return NULL;

  HeapObject* object = current_->GetObject();
  current_ = current_->next();
  return object;
}


// -----------------------------------------------------------------------------
// LargeObjectChunk

LargeObjectChunk* LargeObjectChunk::New(int size_in_bytes,
                                        Executability executable) {
  size_t requested = ChunkSizeFor(size_in_bytes);
  size_t size;
  size_t guard_size = (executable == EXECUTABLE) ? Page::kPageSize : 0;
  Isolate* isolate = Isolate::Current();
  void* mem = isolate->memory_allocator()->AllocateRawMemory(
      requested + guard_size, &size, executable);
  if (mem == NULL) return NULL;

  // The start of the chunk may be overlayed with a page so we have to
  // make sure that the page flags fit in the size field.
  ASSERT((size & Page::kPageFlagMask) == 0);

  LOG(isolate, NewEvent("LargeObjectChunk", mem, size));
  if (size < requested + guard_size) {
    isolate->memory_allocator()->FreeRawMemory(
        mem, size, executable);
    LOG(isolate, DeleteEvent("LargeObjectChunk", mem));
    return NULL;
  }

  if (guard_size != 0) {
    OS::Guard(mem, guard_size);
    size -= guard_size;
    mem = static_cast<Address>(mem) + guard_size;
  }

  ObjectSpace space = (executable == EXECUTABLE)
      ? kObjectSpaceCodeSpace
      : kObjectSpaceLoSpace;
  isolate->memory_allocator()->PerformAllocationCallback(
      space, kAllocationActionAllocate, size);

  LargeObjectChunk* chunk = reinterpret_cast<LargeObjectChunk*>(mem);
  chunk->size_ = size;
  chunk->GetPage()->heap_ = isolate->heap();
  return chunk;
}


void LargeObjectChunk::Free(Executability executable) {
  size_t guard_size = (executable == EXECUTABLE) ? Page::kPageSize : 0;
  ObjectSpace space =
      (executable == EXECUTABLE) ? kObjectSpaceCodeSpace : kObjectSpaceLoSpace;
  // Do not access instance fields after FreeRawMemory!
  Address my_address = address();
  size_t my_size = size();
  Isolate* isolate = GetPage()->heap_->isolate();
  MemoryAllocator* a = isolate->memory_allocator();
  a->FreeRawMemory(my_address - guard_size, my_size + guard_size, executable);
  a->PerformAllocationCallback(space, kAllocationActionFree, my_size);
  LOG(isolate, DeleteEvent("LargeObjectChunk", my_address));
}


int LargeObjectChunk::ChunkSizeFor(int size_in_bytes) {
  int os_alignment = static_cast<int>(OS::AllocateAlignment());
  if (os_alignment < Page::kPageSize) {
    size_in_bytes += (Page::kPageSize - os_alignment);
  }
  return size_in_bytes + Page::kObjectStartOffset;
}

// -----------------------------------------------------------------------------
// LargeObjectSpace

LargeObjectSpace::LargeObjectSpace(Heap* heap, AllocationSpace id)
    : Space(heap, id, NOT_EXECUTABLE),  // Managed on a per-allocation basis
      first_chunk_(NULL),
      size_(0),
      page_count_(0),
      objects_size_(0) {}


bool LargeObjectSpace::Setup() {
  first_chunk_ = NULL;
  size_ = 0;
  page_count_ = 0;
  objects_size_ = 0;
  return true;
}


void LargeObjectSpace::TearDown() {
  while (first_chunk_ != NULL) {
    LargeObjectChunk* chunk = first_chunk_;
    first_chunk_ = first_chunk_->next();
    chunk->Free(chunk->GetPage()->PageExecutability());
  }
  Setup();
}


MaybeObject* LargeObjectSpace::AllocateRawInternal(int requested_size,
                                                   int object_size,
                                                   Executability executable) {
  ASSERT(0 < object_size && object_size <= requested_size);

  // Check if we want to force a GC before growing the old space further.
  // If so, fail the allocation.
  if (!heap()->always_allocate() &&
      heap()->OldGenerationAllocationLimitReached()) {
    return Failure::RetryAfterGC(identity());
  }

  LargeObjectChunk* chunk = LargeObjectChunk::New(requested_size, executable);
  if (chunk == NULL) {
    return Failure::RetryAfterGC(identity());
  }

  size_ += static_cast<int>(chunk->size());
  objects_size_ += requested_size;
  page_count_++;
  chunk->set_next(first_chunk_);
  first_chunk_ = chunk;

  // Initialize page header.
  Page* page = chunk->GetPage();
  Address object_address = page->ObjectAreaStart();

  // Clear the low order bit of the second word in the page to flag it as a
  // large object page.  If the chunk_size happened to be written there, its
  // low order bit should already be clear.
  page->SetIsLargeObjectPage(true);
  page->SetPageExecutability(executable);
  page->SetRegionMarks(Page::kAllRegionsCleanMarks);
  return HeapObject::FromAddress(object_address);
}


MaybeObject* LargeObjectSpace::AllocateRawCode(int size_in_bytes) {
  ASSERT(0 < size_in_bytes);
  return AllocateRawInternal(size_in_bytes,
                             size_in_bytes,
                             EXECUTABLE);
}


MaybeObject* LargeObjectSpace::AllocateRawFixedArray(int size_in_bytes) {
  ASSERT(0 < size_in_bytes);
  return AllocateRawInternal(size_in_bytes,
                             size_in_bytes,
                             NOT_EXECUTABLE);
}


MaybeObject* LargeObjectSpace::AllocateRaw(int size_in_bytes) {
  ASSERT(0 < size_in_bytes);
  return AllocateRawInternal(size_in_bytes,
                             size_in_bytes,
                             NOT_EXECUTABLE);
}


// GC support
MaybeObject* LargeObjectSpace::FindObject(Address a) {
  for (LargeObjectChunk* chunk = first_chunk_;
       chunk != NULL;
       chunk = chunk->next()) {
    Address chunk_address = chunk->address();
    if (chunk_address <= a && a < chunk_address + chunk->size()) {
      return chunk->GetObject();
    }
  }
  return Failure::Exception();
}


LargeObjectChunk* LargeObjectSpace::FindChunkContainingPc(Address pc) {
  // TODO(853): Change this implementation to only find executable
  // chunks and use some kind of hash-based approach to speed it up.
  for (LargeObjectChunk* chunk = first_chunk_;
       chunk != NULL;
       chunk = chunk->next()) {
    Address chunk_address = chunk->address();
    if (chunk_address <= pc && pc < chunk_address + chunk->size()) {
      return chunk;
    }
  }
  return NULL;
}


void LargeObjectSpace::IterateDirtyRegions(ObjectSlotCallback copy_object) {
  LargeObjectIterator it(this);
  for (HeapObject* object = it.next(); object != NULL; object = it.next()) {
    // We only have code, sequential strings, or fixed arrays in large
    // object space, and only fixed arrays can possibly contain pointers to
    // the young generation.
    if (object->IsFixedArray()) {
      Page* page = Page::FromAddress(object->address());
      uint32_t marks = page->GetRegionMarks();
      uint32_t newmarks = Page::kAllRegionsCleanMarks;

      if (marks != Page::kAllRegionsCleanMarks) {
        // For a large page a single dirty mark corresponds to several
        // regions (modulo 32). So we treat a large page as a sequence of
        // normal pages of size Page::kPageSize having same dirty marks
        // and subsequently iterate dirty regions on each of these pages.
        Address start = object->address();
        Address end = page->ObjectAreaEnd();
        Address object_end = start + object->Size();

        // Iterate regions of the first normal page covering object.
        uint32_t first_region_number = page->GetRegionNumberForAddress(start);
        newmarks |=
            heap()->IterateDirtyRegions(marks >> first_region_number,
                                        start,
                                        end,
                                        &Heap::IteratePointersInDirtyRegion,
                                        copy_object) << first_region_number;

        start = end;
        end = start + Page::kPageSize;
        while (end <= object_end) {
          // Iterate next 32 regions.
          newmarks |=
              heap()->IterateDirtyRegions(marks,
                                          start,
                                          end,
                                          &Heap::IteratePointersInDirtyRegion,
                                          copy_object);
          start = end;
          end = start + Page::kPageSize;
        }

        if (start != object_end) {
          // Iterate the last piece of an object which is less than
          // Page::kPageSize.
          newmarks |=
              heap()->IterateDirtyRegions(marks,
                                          start,
                                          object_end,
                                          &Heap::IteratePointersInDirtyRegion,
                                          copy_object);
        }

        page->SetRegionMarks(newmarks);
      }
    }
  }
}


void LargeObjectSpace::FreeUnmarkedObjects() {
  LargeObjectChunk* previous = NULL;
  LargeObjectChunk* current = first_chunk_;
  while (current != NULL) {
    HeapObject* object = current->GetObject();
    if (object->IsMarked()) {
      object->ClearMark();
      heap()->mark_compact_collector()->tracer()->decrement_marked_count();
      previous = current;
      current = current->next();
    } else {
      // Cut the chunk out from the chunk list.
      LargeObjectChunk* current_chunk = current;
      current = current->next();
      if (previous == NULL) {
        first_chunk_ = current;
      } else {
        previous->set_next(current);
      }

      // Free the chunk.
      heap()->mark_compact_collector()->ReportDeleteIfNeeded(
          object, heap()->isolate());
      LiveObjectList::ProcessNonLive(object);

      size_ -= static_cast<int>(current_chunk->size());
      objects_size_ -= object->Size();
      page_count_--;
      current_chunk->Free(current_chunk->GetPage()->PageExecutability());
    }
  }
}


bool LargeObjectSpace::Contains(HeapObject* object) {
  Address address = object->address();
  if (heap()->new_space()->Contains(address)) {
    return false;
  }
  Page* page = Page::FromAddress(address);

  SLOW_ASSERT(!page->IsLargeObjectPage()
              || !FindObject(address)->IsFailure());

  return page->IsLargeObjectPage();
}


#ifdef DEBUG
// We do not assume that the large object iterator works, because it depends
// on the invariants we are checking during verification.
void LargeObjectSpace::Verify() {
  for (LargeObjectChunk* chunk = first_chunk_;
       chunk != NULL;
       chunk = chunk->next()) {
    // Each chunk contains an object that starts at the large object page's
    // object area start.
    HeapObject* object = chunk->GetObject();
    Page* page = Page::FromAddress(object->address());
    ASSERT(object->address() == page->ObjectAreaStart());

    // The first word should be a map, and we expect all map pointers to be
    // in map space.
    Map* map = object->map();
    ASSERT(map->IsMap());
    ASSERT(heap()->map_space()->Contains(map));

    // We have only code, sequential strings, external strings
    // (sequential strings that have been morphed into external
    // strings), fixed arrays, and byte arrays in large object space.
    ASSERT(object->IsCode() || object->IsSeqString() ||
           object->IsExternalString() || object->IsFixedArray() ||
           object->IsFixedDoubleArray() || object->IsByteArray());

    // The object itself should look OK.
    object->Verify();

    // Byte arrays and strings don't have interior pointers.
    if (object->IsCode()) {
      VerifyPointersVisitor code_visitor;
      object->IterateBody(map->instance_type(),
                          object->Size(),
                          &code_visitor);
    } else if (object->IsFixedArray()) {
      // We loop over fixed arrays ourselves, rather then using the visitor,
      // because the visitor doesn't support the start/offset iteration
      // needed for IsRegionDirty.
      FixedArray* array = FixedArray::cast(object);
      for (int j = 0; j < array->length(); j++) {
        Object* element = array->get(j);
        if (element->IsHeapObject()) {
          HeapObject* element_object = HeapObject::cast(element);
          ASSERT(heap()->Contains(element_object));
          ASSERT(element_object->map()->IsMap());
          if (heap()->InNewSpace(element_object)) {
            Address array_addr = object->address();
            Address element_addr = array_addr + FixedArray::kHeaderSize +
                j * kPointerSize;

            ASSERT(Page::FromAddress(array_addr)->IsRegionDirty(element_addr));
          }
        }
      }
    }
  }
}


void LargeObjectSpace::Print() {
  LargeObjectIterator it(this);
  for (HeapObject* obj = it.next(); obj != NULL; obj = it.next()) {
    obj->Print();
  }
}


void LargeObjectSpace::ReportStatistics() {
  PrintF("  size: %" V8_PTR_PREFIX "d\n", size_);
  int num_objects = 0;
  ClearHistograms();
  LargeObjectIterator it(this);
  for (HeapObject* obj = it.next(); obj != NULL; obj = it.next()) {
    num_objects++;
    CollectHistogramInfo(obj);
  }

  PrintF("  number of objects %d, "
         "size of objects %" V8_PTR_PREFIX "d\n", num_objects, objects_size_);
  if (num_objects > 0) ReportHistogram(false);
}


void LargeObjectSpace::CollectCodeStatistics() {
  Isolate* isolate = heap()->isolate();
  LargeObjectIterator obj_it(this);
  for (HeapObject* obj = obj_it.next(); obj != NULL; obj = obj_it.next()) {
    if (obj->IsCode()) {
      Code* code = Code::cast(obj);
      isolate->code_kind_statistics()[code->kind()] += code->Size();
    }
  }
}
#endif  // DEBUG

} }  // namespace v8::internal
