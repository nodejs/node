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

#include "v8.h"

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


bool HeapObjectIterator::HasNextInNextPage() {
  if (cur_addr_ == end_addr_) return false;

  Page* cur_page = Page::FromAllocationTop(cur_addr_);
  cur_page = cur_page->next_page();
  ASSERT(cur_page->is_valid());

  cur_addr_ = cur_page->ObjectAreaStart();
  cur_limit_ = (cur_page == end_page_) ? end_addr_ : cur_page->AllocationTop();

  ASSERT(cur_addr_ < cur_limit_);
#ifdef DEBUG
  Verify();
#endif
  return true;
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
// Page

#ifdef DEBUG
Page::RSetState Page::rset_state_ = Page::IN_USE;
#endif

// -----------------------------------------------------------------------------
// MemoryAllocator
//
int MemoryAllocator::capacity_   = 0;
int MemoryAllocator::size_       = 0;

VirtualMemory* MemoryAllocator::initial_chunk_ = NULL;

// 270 is an estimate based on the static default heap size of a pair of 256K
// semispaces and a 64M old generation.
const int kEstimatedNumberOfChunks = 270;
List<MemoryAllocator::ChunkInfo> MemoryAllocator::chunks_(
    kEstimatedNumberOfChunks);
List<int> MemoryAllocator::free_chunk_ids_(kEstimatedNumberOfChunks);
int MemoryAllocator::max_nof_chunks_ = 0;
int MemoryAllocator::top_ = 0;


void MemoryAllocator::Push(int free_chunk_id) {
  ASSERT(max_nof_chunks_ > 0);
  ASSERT(top_ < max_nof_chunks_);
  free_chunk_ids_[top_++] = free_chunk_id;
}


int MemoryAllocator::Pop() {
  ASSERT(top_ > 0);
  return free_chunk_ids_[--top_];
}


bool MemoryAllocator::Setup(int capacity) {
  capacity_ = RoundUp(capacity, Page::kPageSize);

  // Over-estimate the size of chunks_ array.  It assumes the expansion of old
  // space is always in the unit of a chunk (kChunkSize) except the last
  // expansion.
  //
  // Due to alignment, allocated space might be one page less than required
  // number (kPagesPerChunk) of pages for old spaces.
  //
  // Reserve two chunk ids for semispaces, one for map space, one for old
  // space, and one for code space.
  max_nof_chunks_ = (capacity_ / (kChunkSize - Page::kPageSize)) + 5;
  if (max_nof_chunks_ > kMaxNofChunks) return false;

  size_ = 0;
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
    LOG(DeleteEvent("InitialChunk", initial_chunk_->address()));
    delete initial_chunk_;
    initial_chunk_ = NULL;
  }

  ASSERT(top_ == max_nof_chunks_);  // all chunks are free
  top_ = 0;
  capacity_ = 0;
  size_ = 0;
  max_nof_chunks_ = 0;
}


void* MemoryAllocator::AllocateRawMemory(const size_t requested,
                                         size_t* allocated,
                                         Executability executable) {
  if (size_ + static_cast<int>(requested) > capacity_) return NULL;

  void* mem = OS::Allocate(requested, allocated, executable == EXECUTABLE);
  int alloced = *allocated;
  size_ += alloced;
  Counters::memory_allocated.Increment(alloced);
  return mem;
}


void MemoryAllocator::FreeRawMemory(void* mem, size_t length) {
  OS::Free(mem, length);
  Counters::memory_allocated.Decrement(length);
  size_ -= length;
  ASSERT(size_ >= 0);
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
  LOG(NewEvent("InitialChunk", initial_chunk_->address(), requested));
  size_ += requested;
  return initial_chunk_->address();
}


static int PagesInChunk(Address start, size_t size) {
  // The first page starts on the first page-aligned address from start onward
  // and the last page ends on the last page-aligned address before
  // start+size.  Page::kPageSize is a power of two so we can divide by
  // shifting.
  return (RoundDown(start + size, Page::kPageSize)
          - RoundUp(start, Page::kPageSize)) >> Page::kPageSizeBits;
}


Page* MemoryAllocator::AllocatePages(int requested_pages, int* allocated_pages,
                                     PagedSpace* owner) {
  if (requested_pages <= 0) return Page::FromAddress(NULL);
  size_t chunk_size = requested_pages * Page::kPageSize;

  // There is not enough space to guarantee the desired number pages can be
  // allocated.
  if (size_ + static_cast<int>(chunk_size) > capacity_) {
    // Request as many pages as we can.
    chunk_size = capacity_ - size_;
    requested_pages = chunk_size >> Page::kPageSizeBits;

    if (requested_pages <= 0) return Page::FromAddress(NULL);
  }
  void* chunk = AllocateRawMemory(chunk_size, &chunk_size, owner->executable());
  if (chunk == NULL) return Page::FromAddress(NULL);
  LOG(NewEvent("PagedChunk", chunk, chunk_size));

  *allocated_pages = PagesInChunk(static_cast<Address>(chunk), chunk_size);
  if (*allocated_pages == 0) {
    FreeRawMemory(chunk, chunk_size);
    LOG(DeleteEvent("PagedChunk", chunk));
    return Page::FromAddress(NULL);
  }

  int chunk_id = Pop();
  chunks_[chunk_id].init(static_cast<Address>(chunk), chunk_size, owner);

  return InitializePagesInChunk(chunk_id, *allocated_pages, owner);
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
  Counters::memory_allocated.Increment(size);

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
  Counters::memory_allocated.Increment(size);
  return true;
}

bool MemoryAllocator::UncommitBlock(Address start, size_t size) {
  ASSERT(start != NULL);
  ASSERT(size > 0);
  ASSERT(initial_chunk_ != NULL);
  ASSERT(InInitialChunk(start));
  ASSERT(InInitialChunk(start + size - 1));

  if (!initial_chunk_->Uncommit(start, size)) return false;
  Counters::memory_allocated.Decrement(size);
  return true;
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
    p->opaque_header = OffsetFrom(page_addr + Page::kPageSize) | chunk_id;
    p->is_normal_page = 1;
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
    Counters::memory_allocated.Decrement(c.size());
  } else {
    LOG(DeleteEvent("PagedChunk", c.address()));
    FreeRawMemory(c.address(), c.size());
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
  PrintF("  capacity: %d, used: %d, available: %%%d\n\n",
         capacity_, size_, static_cast<int>(pct*100));
}
#endif


// -----------------------------------------------------------------------------
// PagedSpace implementation

PagedSpace::PagedSpace(int max_capacity,
                       AllocationSpace id,
                       Executability executable)
    : Space(id, executable) {
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
    first_page_ = MemoryAllocator::CommitPages(RoundUp(start, Page::kPageSize),
                                               Page::kPageSize * pages_in_chunk,
                                               this, &num_pages);
  } else {
    int requested_pages = Min(MemoryAllocator::kPagesPerChunk,
                              max_capacity_ / Page::kObjectAreaSize);
    first_page_ =
        MemoryAllocator::AllocatePages(requested_pages, &num_pages, this);
    if (!first_page_->is_valid()) return false;
  }

  // We are sure that the first page is valid and that we have at least one
  // page.
  ASSERT(first_page_->is_valid());
  ASSERT(num_pages > 0);
  accounting_stats_.ExpandSpace(num_pages * Page::kObjectAreaSize);
  ASSERT(Capacity() <= max_capacity_);

  // Sequentially initialize remembered sets in the newly allocated
  // pages and cache the current last page in the space.
  for (Page* p = first_page_; p->is_valid(); p = p->next_page()) {
    p->ClearRSet();
    last_page_ = p;
  }

  // Use first_page_ for allocation.
  SetAllocationInfo(&allocation_info_, first_page_);

  return true;
}


bool PagedSpace::HasBeenSetup() {
  return (Capacity() > 0);
}


void PagedSpace::TearDown() {
  first_page_ = MemoryAllocator::FreePages(first_page_);
  ASSERT(!first_page_->is_valid());

  accounting_stats_.Clear();
}


#ifdef ENABLE_HEAP_PROTECTION

void PagedSpace::Protect() {
  Page* page = first_page_;
  while (page->is_valid()) {
    MemoryAllocator::ProtectChunkFromPage(page);
    page = MemoryAllocator::FindLastPageInSameChunk(page)->next_page();
  }
}


void PagedSpace::Unprotect() {
  Page* page = first_page_;
  while (page->is_valid()) {
    MemoryAllocator::UnprotectChunkFromPage(page);
    page = MemoryAllocator::FindLastPageInSameChunk(page)->next_page();
  }
}

#endif


void PagedSpace::ClearRSet() {
  PageIterator it(this, PageIterator::ALL_PAGES);
  while (it.has_next()) {
    it.next()->ClearRSet();
  }
}


Object* PagedSpace::FindObject(Address addr) {
  // Note: this function can only be called before or after mark-compact GC
  // because it accesses map pointers.
  ASSERT(!MarkCompactCollector::in_use());

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
  current_page->mc_relocation_top = mc_forwarding_info_.top;
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

  int available_pages = (max_capacity_ - Capacity()) / Page::kObjectAreaSize;
  if (available_pages <= 0) return false;

  int desired_pages = Min(available_pages, MemoryAllocator::kPagesPerChunk);
  Page* p = MemoryAllocator::AllocatePages(desired_pages, &desired_pages, this);
  if (!p->is_valid()) return false;

  accounting_stats_.ExpandSpace(desired_pages * Page::kObjectAreaSize);
  ASSERT(Capacity() <= max_capacity_);

  MemoryAllocator::SetNextPage(last_page, p);

  // Sequentially clear remembered set of new pages and and cache the
  // new last page in the space.
  while (p->is_valid()) {
    p->ClearRSet();
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
  // Release half of free pages.
  Page* top_page = AllocationTopPage();
  ASSERT(top_page->is_valid());

  // Count the number of pages we would like to free.
  int pages_to_free = 0;
  for (Page* p = top_page->next_page(); p->is_valid(); p = p->next_page()) {
    pages_to_free++;
  }

  // Free pages after top_page.
  Page* p = MemoryAllocator::FreePages(top_page->next_page());
  MemoryAllocator::SetNextPage(top_page, p);

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
    last_page = MemoryAllocator::FindLastPageInSameChunk(next_page);
    next_page = last_page->next_page();
  }

  // Expand the space until it has the required capacity or expansion fails.
  do {
    if (!Expand(last_page)) return false;
    ASSERT(last_page->next_page()->is_valid());
    last_page =
        MemoryAllocator::FindLastPageInSameChunk(last_page->next_page());
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
  ASSERT(MemoryAllocator::IsPageInSpace(top_page, this));

  // Loop over all the pages.
  bool above_allocation_top = false;
  Page* current_page = first_page_;
  while (current_page->is_valid()) {
    if (above_allocation_top) {
      // We don't care what's above the allocation top.
    } else {
      // Unless this is the last page in the space containing allocated
      // objects, the allocation top should be at a constant offset from the
      // object area end.
      Address top = current_page->AllocationTop();
      if (current_page == top_page) {
        ASSERT(top == allocation_info_.top);
        // The next page will be above the allocation top.
        above_allocation_top = true;
      } else {
        ASSERT(top == current_page->ObjectAreaEnd() - page_extra_);
      }

      // It should be packed with objects from the bottom to the top.
      Address current = current_page->ObjectAreaStart();
      while (current < top) {
        HeapObject* object = HeapObject::FromAddress(current);

        // The first word should be a map, and we expect all map pointers to
        // be in map space.
        Map* map = object->map();
        ASSERT(map->IsMap());
        ASSERT(Heap::map_space()->Contains(map));

        // Perform space-specific object verification.
        VerifyObject(object);

        // The object itself should look OK.
        object->Verify();

        // All the interior pointers should be contained in the heap and
        // have their remembered set bits set if required as determined
        // by the visitor.
        int size = object->Size();
        if (object->IsCode()) {
          Code::cast(object)->ConvertICTargetsFromAddressToObject();
          object->IterateBody(map->instance_type(), size, visitor);
          Code::cast(object)->ConvertICTargetsFromObjectToAddress();
        } else {
          object->IterateBody(map->instance_type(), size, visitor);
        }

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
  int initial_semispace_capacity = Heap::InitialSemiSpaceSize();
  int maximum_semispace_capacity = Heap::SemiSpaceSize();

  ASSERT(initial_semispace_capacity <= maximum_semispace_capacity);
  ASSERT(IsPowerOf2(maximum_semispace_capacity));

  // Allocate and setup the histogram arrays if necessary.
#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
  allocated_histogram_ = NewArray<HistogramInfo>(LAST_TYPE + 1);
  promoted_histogram_ = NewArray<HistogramInfo>(LAST_TYPE + 1);

#define SET_NAME(name) allocated_histogram_[name].set_name(#name); \
                       promoted_histogram_[name].set_name(#name);
  INSTANCE_TYPE_LIST(SET_NAME)
#undef SET_NAME
#endif

  ASSERT(size == 2 * maximum_semispace_capacity);
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
  object_mask_ = address_mask_ | kHeapObjectTag;
  object_expected_ = reinterpret_cast<uintptr_t>(start) | kHeapObjectTag;

  allocation_info_.top = to_space_.low();
  allocation_info_.limit = to_space_.high();
  mc_forwarding_info_.top = NULL;
  mc_forwarding_info_.limit = NULL;

  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
  return true;
}


void NewSpace::TearDown() {
#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
  if (allocated_histogram_) {
    DeleteArray(allocated_histogram_);
    allocated_histogram_ = NULL;
  }
  if (promoted_histogram_) {
    DeleteArray(promoted_histogram_);
    promoted_histogram_ = NULL;
  }
#endif

  start_ = NULL;
  allocation_info_.top = NULL;
  allocation_info_.limit = NULL;
  mc_forwarding_info_.top = NULL;
  mc_forwarding_info_.limit = NULL;

  to_space_.TearDown();
  from_space_.TearDown();
}


#ifdef ENABLE_HEAP_PROTECTION

void NewSpace::Protect() {
  MemoryAllocator::Protect(ToSpaceLow(), Capacity());
  MemoryAllocator::Protect(FromSpaceLow(), Capacity());
}


void NewSpace::Unprotect() {
  MemoryAllocator::Unprotect(ToSpaceLow(), Capacity(),
                             to_space_.executable());
  MemoryAllocator::Unprotect(FromSpaceLow(), Capacity(),
                             from_space_.executable());
}

#endif


void NewSpace::Flip() {
  SemiSpace tmp = from_space_;
  from_space_ = to_space_;
  to_space_ = tmp;
}


bool NewSpace::Grow() {
  ASSERT(Capacity() < MaximumCapacity());
  // TODO(1240712): Failure to double the from space can result in
  // semispaces of different sizes.  In the event of that failure, the
  // to space doubling should be rolled back before returning false.
  if (!to_space_.Grow() || !from_space_.Grow()) return false;
  allocation_info_.limit = to_space_.high();
  ASSERT_SEMISPACE_ALLOCATION_INFO(allocation_info_, to_space_);
  return true;
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
    ASSERT(Heap::map_space()->Contains(map));

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
  if (!MemoryAllocator::CommitBlock(start_, capacity_, executable())) {
    return false;
  }
  committed_ = true;
  return true;
}


bool SemiSpace::Uncommit() {
  ASSERT(is_committed());
  if (!MemoryAllocator::UncommitBlock(start_, capacity_)) {
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
  capacity_ = initial_capacity;
  maximum_capacity_ = maximum_capacity;
  committed_ = false;

  start_ = start;
  address_mask_ = ~(maximum_capacity - 1);
  object_mask_ = address_mask_ | kHeapObjectTag;
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
  int extra = Min(RoundUp(capacity_, OS::AllocateAlignment()),
                  maximum_extra);
  if (!MemoryAllocator::CommitBlock(high(), extra, executable())) {
    return false;
  }
  capacity_ += extra;
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
// A static array of histogram info for each type.
static HistogramInfo heap_histograms[LAST_TYPE+1];
static JSObject::SpillInformation js_spill_information;

// heap_histograms is shared, always clear it before using it.
static void ClearHistograms() {
  // We reset the name each time, though it hasn't changed.
#define DEF_TYPE_NAME(name) heap_histograms[name].set_name(#name);
  INSTANCE_TYPE_LIST(DEF_TYPE_NAME)
#undef DEF_TYPE_NAME

#define CLEAR_HISTOGRAM(name) heap_histograms[name].clear();
  INSTANCE_TYPE_LIST(CLEAR_HISTOGRAM)
#undef CLEAR_HISTOGRAM

  js_spill_information.Clear();
}


static int code_kind_statistics[Code::NUMBER_OF_KINDS];


static void ClearCodeKindStatistics() {
  for (int i = 0; i < Code::NUMBER_OF_KINDS; i++) {
    code_kind_statistics[i] = 0;
  }
}


static void ReportCodeKindStatistics() {
  const char* table[Code::NUMBER_OF_KINDS];

#define CASE(name)                            \
  case Code::name: table[Code::name] = #name; \
  break

  for (int i = 0; i < Code::NUMBER_OF_KINDS; i++) {
    switch (static_cast<Code::Kind>(i)) {
      CASE(FUNCTION);
      CASE(STUB);
      CASE(BUILTIN);
      CASE(LOAD_IC);
      CASE(KEYED_LOAD_IC);
      CASE(STORE_IC);
      CASE(KEYED_STORE_IC);
      CASE(CALL_IC);
    }
  }

#undef CASE

  PrintF("\n   Code kind histograms: \n");
  for (int i = 0; i < Code::NUMBER_OF_KINDS; i++) {
    if (code_kind_statistics[i] > 0) {
      PrintF("     %-20s: %10d bytes\n", table[i], code_kind_statistics[i]);
    }
  }
  PrintF("\n");
}


static int CollectHistogramInfo(HeapObject* obj) {
  InstanceType type = obj->map()->instance_type();
  ASSERT(0 <= type && type <= LAST_TYPE);
  ASSERT(heap_histograms[type].name() != NULL);
  heap_histograms[type].increment_number(1);
  heap_histograms[type].increment_bytes(obj->Size());

  if (FLAG_collect_heap_spill_statistics && obj->IsJSObject()) {
    JSObject::cast(obj)->IncrementSpillStatistics(&js_spill_information);
  }

  return obj->Size();
}


static void ReportHistogram(bool print_spill) {
  PrintF("\n  Object Histogram:\n");
  for (int i = 0; i <= LAST_TYPE; i++) {
    if (heap_histograms[i].number() > 0) {
      PrintF("    %-33s%10d (%10d bytes)\n",
             heap_histograms[i].name(),
             heap_histograms[i].number(),
             heap_histograms[i].bytes());
    }
  }
  PrintF("\n");

  // Summarize string types.
  int string_number = 0;
  int string_bytes = 0;
#define INCREMENT(type, size, name, camel_name)      \
    string_number += heap_histograms[type].number(); \
    string_bytes += heap_histograms[type].bytes();
  STRING_TYPE_LIST(INCREMENT)
#undef INCREMENT
  if (string_number > 0) {
    PrintF("    %-33s%10d (%10d bytes)\n\n", "STRING_TYPE", string_number,
           string_bytes);
  }

  if (FLAG_collect_heap_spill_statistics && print_spill) {
    js_spill_information.Print();
  }
}
#endif  // DEBUG


// Support for statistics gathering for --heap-stats and --log-gc.
#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
void NewSpace::ClearHistograms() {
  for (int i = 0; i <= LAST_TYPE; i++) {
    allocated_histogram_[i].clear();
    promoted_histogram_[i].clear();
  }
}

// Because the copying collector does not touch garbage objects, we iterate
// the new space before a collection to get a histogram of allocated objects.
// This only happens (1) when compiled with DEBUG and the --heap-stats flag is
// set, or when compiled with ENABLE_LOGGING_AND_PROFILING and the --log-gc
// flag is set.
void NewSpace::CollectStatistics() {
  ClearHistograms();
  SemiSpaceIterator it(this);
  while (it.has_next()) RecordAllocation(it.next());
}


#ifdef ENABLE_LOGGING_AND_PROFILING
static void DoReportStatistics(HistogramInfo* info, const char* description) {
  LOG(HeapSampleBeginEvent("NewSpace", description));
  // Lump all the string types together.
  int string_number = 0;
  int string_bytes = 0;
#define INCREMENT(type, size, name, camel_name)       \
    string_number += info[type].number();             \
    string_bytes += info[type].bytes();
  STRING_TYPE_LIST(INCREMENT)
#undef INCREMENT
  if (string_number > 0) {
    LOG(HeapSampleItemEvent("STRING_TYPE", string_number, string_bytes));
  }

  // Then do the other types.
  for (int i = FIRST_NONSTRING_TYPE; i <= LAST_TYPE; ++i) {
    if (info[i].number() > 0) {
      LOG(HeapSampleItemEvent(info[i].name(), info[i].number(),
                              info[i].bytes()));
    }
  }
  LOG(HeapSampleEndEvent("NewSpace", description));
}
#endif  // ENABLE_LOGGING_AND_PROFILING


void NewSpace::ReportStatistics() {
#ifdef DEBUG
  if (FLAG_heap_stats) {
    float pct = static_cast<float>(Available()) / Capacity();
    PrintF("  capacity: %d, available: %d, %%%d\n",
           Capacity(), Available(), static_cast<int>(pct*100));
    PrintF("\n  Object Histogram:\n");
    for (int i = 0; i <= LAST_TYPE; i++) {
      if (allocated_histogram_[i].number() > 0) {
        PrintF("    %-33s%10d (%10d bytes)\n",
               allocated_histogram_[i].name(),
               allocated_histogram_[i].number(),
               allocated_histogram_[i].bytes());
      }
    }
    PrintF("\n");
  }
#endif  // DEBUG

#ifdef ENABLE_LOGGING_AND_PROFILING
  if (FLAG_log_gc) {
    DoReportStatistics(allocated_histogram_, "allocated");
    DoReportStatistics(promoted_histogram_, "promoted");
  }
#endif  // ENABLE_LOGGING_AND_PROFILING
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
#endif  // defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)


// -----------------------------------------------------------------------------
// Free lists for old object spaces implementation

void FreeListNode::set_size(int size_in_bytes) {
  ASSERT(size_in_bytes > 0);
  ASSERT(IsAligned(size_in_bytes, kPointerSize));

  // We write a map and possibly size information to the block.  If the block
  // is big enough to be a ByteArray with at least one extra word (the next
  // pointer), we set its map to be the byte array map and its size to an
  // appropriate array length for the desired size from HeapObject::Size().
  // If the block is too small (eg, one or two words), to hold both a size
  // field and a next pointer, we give it a filler map that gives it the
  // correct size.
  if (size_in_bytes > ByteArray::kAlignedSize) {
    set_map(Heap::raw_unchecked_byte_array_map());
    ByteArray::cast(this)->set_length(ByteArray::LengthFor(size_in_bytes));
  } else if (size_in_bytes == kPointerSize) {
    set_map(Heap::raw_unchecked_one_pointer_filler_map());
  } else if (size_in_bytes == 2 * kPointerSize) {
    set_map(Heap::raw_unchecked_two_pointer_filler_map());
  } else {
    UNREACHABLE();
  }
  ASSERT(Size() == size_in_bytes);
}


Address FreeListNode::next() {
  ASSERT(map() == Heap::raw_unchecked_byte_array_map() ||
         map() == Heap::raw_unchecked_two_pointer_filler_map());
  if (map() == Heap::raw_unchecked_byte_array_map()) {
    ASSERT(Size() >= kNextOffset + kPointerSize);
    return Memory::Address_at(address() + kNextOffset);
  } else {
    return Memory::Address_at(address() + kPointerSize);
  }
}


void FreeListNode::set_next(Address next) {
  ASSERT(map() == Heap::raw_unchecked_byte_array_map() ||
         map() == Heap::raw_unchecked_two_pointer_filler_map());
  if (map() == Heap::raw_unchecked_byte_array_map()) {
    ASSERT(Size() >= kNextOffset + kPointerSize);
    Memory::Address_at(address() + kNextOffset) = next;
  } else {
    Memory::Address_at(address() + kPointerSize) = next;
  }
}


OldSpaceFreeList::OldSpaceFreeList(AllocationSpace owner) : owner_(owner) {
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
  for (int i = 0; i < size_in_bytes; i += kPointerSize) {
    Memory::Address_at(start + i) = kZapValue;
  }
#endif
  FreeListNode* node = FreeListNode::FromAddress(start);
  node->set_size(size_in_bytes);

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
  node->set_next(free_[index].head_node_);
  free_[index].head_node_ = node->address();
  available_ += size_in_bytes;
  needs_rebuild_ = true;
  return 0;
}


Object* OldSpaceFreeList::Allocate(int size_in_bytes, int* wasted_bytes) {
  ASSERT(0 < size_in_bytes);
  ASSERT(size_in_bytes <= kMaxBlockSize);
  ASSERT(IsAligned(size_in_bytes, kPointerSize));

  if (needs_rebuild_) RebuildSizeList();
  int index = size_in_bytes >> kPointerSizeLog2;
  // Check for a perfect fit.
  if (free_[index].head_node_ != NULL) {
    FreeListNode* node = FreeListNode::FromAddress(free_[index].head_node_);
    // If this was the last block of its size, remove the size.
    if ((free_[index].head_node_ = node->next()) == NULL) RemoveSize(index);
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
    return Failure::RetryAfterGC(size_in_bytes, owner_);
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
    if ((free_[cur].head_node_ = cur_node->next()) == NULL) {
      free_[rem].next_size_ = free_[cur].next_size_;
    } else {
      free_[rem].next_size_ = cur;
    }
    // Add the remainder block.
    rem_node->set_size(rem_bytes);
    rem_node->set_next(free_[rem].head_node_);
    free_[rem].head_node_ = rem_node->address();
  } else {
    // If this was the last block of size cur, remove the size.
    if ((free_[cur].head_node_ = cur_node->next()) == NULL) {
      finger_ = prev;
      free_[prev].next_size_ = free_[cur].next_size_;
    }
    if (rem_bytes < kMinBlockSize) {
      // Too-small remainder is wasted.
      rem_node->set_size(rem_bytes);
      available_ -= size_in_bytes + rem_bytes;
      *wasted_bytes = rem_bytes;
      return cur_node;
    }
    // Add the remainder block and, if needed, insert its size.
    rem_node->set_size(rem_bytes);
    rem_node->set_next(free_[rem].head_node_);
    free_[rem].head_node_ = rem_node->address();
    if (rem_node->next() == NULL) InsertSize(rem);
  }
  available_ -= size_in_bytes;
  *wasted_bytes = 0;
  return cur_node;
}


#ifdef DEBUG
bool OldSpaceFreeList::Contains(FreeListNode* node) {
  for (int i = 0; i < kFreeListsLength; i++) {
    Address cur_addr = free_[i].head_node_;
    while (cur_addr != NULL) {
      FreeListNode* cur_node = FreeListNode::FromAddress(cur_addr);
      if (cur_node == node) return true;
      cur_addr = cur_node->next();
    }
  }
  return false;
}
#endif


FixedSizeFreeList::FixedSizeFreeList(AllocationSpace owner, int object_size)
    : owner_(owner), object_size_(object_size) {
  Reset();
}


void FixedSizeFreeList::Reset() {
  available_ = 0;
  head_ = NULL;
}


void FixedSizeFreeList::Free(Address start) {
#ifdef DEBUG
  for (int i = 0; i < object_size_; i += kPointerSize) {
    Memory::Address_at(start + i) = kZapValue;
  }
#endif
  ASSERT(!FLAG_always_compact);  // We only use the freelists with mark-sweep.
  FreeListNode* node = FreeListNode::FromAddress(start);
  node->set_size(object_size_);
  node->set_next(head_);
  head_ = node->address();
  available_ += object_size_;
}


Object* FixedSizeFreeList::Allocate() {
  if (head_ == NULL) {
    return Failure::RetryAfterGC(object_size_, owner_);
  }

  ASSERT(!FLAG_always_compact);  // We only use the freelists with mark-sweep.
  FreeListNode* node = FreeListNode::FromAddress(head_);
  head_ = node->next();
  available_ -= object_size_;
  return node;
}


// -----------------------------------------------------------------------------
// OldSpace implementation

void OldSpace::PrepareForMarkCompact(bool will_compact) {
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
    computed_size += p->mc_relocation_top - p->ObjectAreaStart();
    if (it.has_next()) {
      // Free the space at the top of the page.  We cannot use
      // p->mc_relocation_top after the call to Free (because Free will clear
      // remembered set bits).
      int extra_size = p->ObjectAreaEnd() - p->mc_relocation_top;
      if (extra_size > 0) {
        int wasted_bytes = free_list_.Free(p->mc_relocation_top, extra_size);
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

  // There is no next page in this space.  Try free list allocation.
  int wasted_bytes;
  Object* result = free_list_.Allocate(size_in_bytes, &wasted_bytes);
  accounting_stats_.WasteBytes(wasted_bytes);
  if (!result->IsFailure()) {
    accounting_stats_.AllocateBytes(size_in_bytes);
    return HeapObject::cast(result);
  }

  // Free list allocation failed and there is no next page.  Fail if we have
  // hit the old generation size limit that should cause a garbage
  // collection.
  if (!Heap::always_allocate() && Heap::OldGenerationAllocationLimitReached()) {
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


// Add the block at the top of the page to the space's free list, set the
// allocation info to the next page (assumed to be one), and allocate
// linearly there.
HeapObject* OldSpace::AllocateInNextPage(Page* current_page,
                                         int size_in_bytes) {
  ASSERT(current_page->next_page()->is_valid());
  // Add the block at the top of this page to the free list.
  int free_size = current_page->ObjectAreaEnd() - allocation_info_.top;
  if (free_size > 0) {
    int wasted_bytes = free_list_.Free(allocation_info_.top, free_size);
    accounting_stats_.WasteBytes(wasted_bytes);
  }
  SetAllocationInfo(&allocation_info_, current_page->next_page());
  return AllocateLinearly(&allocation_info_, size_in_bytes);
}


#ifdef DEBUG
struct CommentStatistic {
  const char* comment;
  int size;
  int count;
  void Clear() {
    comment = NULL;
    size = 0;
    count = 0;
  }
};


// must be small, since an iteration is used for lookup
const int kMaxComments = 64;
static CommentStatistic comments_statistics[kMaxComments+1];


void PagedSpace::ReportCodeStatistics() {
  ReportCodeKindStatistics();
  PrintF("Code comment statistics (\"   [ comment-txt   :    size/   "
         "count  (average)\"):\n");
  for (int i = 0; i <= kMaxComments; i++) {
    const CommentStatistic& cs = comments_statistics[i];
    if (cs.size > 0) {
      PrintF("   %-30s: %10d/%6d     (%d)\n", cs.comment, cs.size, cs.count,
             cs.size/cs.count);
    }
  }
  PrintF("\n");
}


void PagedSpace::ResetCodeStatistics() {
  ClearCodeKindStatistics();
  for (int i = 0; i < kMaxComments; i++) comments_statistics[i].Clear();
  comments_statistics[kMaxComments].comment = "Unknown";
  comments_statistics[kMaxComments].size = 0;
  comments_statistics[kMaxComments].count = 0;
}


// Adds comment to 'comment_statistics' table. Performance OK sa long as
// 'kMaxComments' is small
static void EnterComment(const char* comment, int delta) {
  // Do not count empty comments
  if (delta <= 0) return;
  CommentStatistic* cs = &comments_statistics[kMaxComments];
  // Search for a free or matching entry in 'comments_statistics': 'cs'
  // points to result.
  for (int i = 0; i < kMaxComments; i++) {
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
static void CollectCommentStatistics(RelocIterator* it) {
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
      flat_delta += it->rinfo()->pc() - prev_pc;
      if (txt[0] == ']') break;  // End of nested  comment
      // A new comment
      CollectCommentStatistics(it);
      // Skip code that was covered with previous comment
      prev_pc = it->rinfo()->pc();
    }
    it->next();
  }
  EnterComment(comment_txt, flat_delta);
}


// Collects code size statistics:
// - by code kind
// - by code comment
void PagedSpace::CollectCodeStatistics() {
  HeapObjectIterator obj_it(this);
  while (obj_it.has_next()) {
    HeapObject* obj = obj_it.next();
    if (obj->IsCode()) {
      Code* code = Code::cast(obj);
      code_kind_statistics[code->kind()] += code->Size();
      RelocIterator it(code);
      int delta = 0;
      const byte* prev_pc = code->instruction_start();
      while (!it.done()) {
        if (it.rinfo()->rmode() == RelocInfo::COMMENT) {
          delta += it.rinfo()->pc() - prev_pc;
          CollectCommentStatistics(&it);
          prev_pc = it.rinfo()->pc();
        }
        it.next();
      }

      ASSERT(code->instruction_start() <= prev_pc &&
             prev_pc <= code->relocation_start());
      delta += code->relocation_start() - prev_pc;
      EnterComment("NoComment", delta);
    }
  }
}


void OldSpace::ReportStatistics() {
  int pct = Available() * 100 / Capacity();
  PrintF("  capacity: %d, waste: %d, available: %d, %%%d\n",
         Capacity(), Waste(), Available(), pct);

  // Report remembered set statistics.
  int rset_marked_pointers = 0;
  int rset_marked_arrays = 0;
  int rset_marked_array_elements = 0;
  int cross_gen_pointers = 0;
  int cross_gen_array_elements = 0;

  PageIterator page_it(this, PageIterator::PAGES_IN_USE);
  while (page_it.has_next()) {
    Page* p = page_it.next();

    for (Address rset_addr = p->RSetStart();
         rset_addr < p->RSetEnd();
         rset_addr += kIntSize) {
      int rset = Memory::int_at(rset_addr);
      if (rset != 0) {
        // Bits were set
        int intoff = rset_addr - p->address();
        int bitoff = 0;
        for (; bitoff < kBitsPerInt; ++bitoff) {
          if ((rset & (1 << bitoff)) != 0) {
            int bitpos = intoff*kBitsPerByte + bitoff;
            Address slot = p->OffsetToAddress(bitpos << kObjectAlignmentBits);
            Object** obj = reinterpret_cast<Object**>(slot);
            if (*obj == Heap::raw_unchecked_fixed_array_map()) {
              rset_marked_arrays++;
              FixedArray* fa = FixedArray::cast(HeapObject::FromAddress(slot));

              rset_marked_array_elements += fa->length();
              // Manually inline FixedArray::IterateBody
              Address elm_start = slot + FixedArray::kHeaderSize;
              Address elm_stop = elm_start + fa->length() * kPointerSize;
              for (Address elm_addr = elm_start;
                   elm_addr < elm_stop; elm_addr += kPointerSize) {
                // Filter non-heap-object pointers
                Object** elm_p = reinterpret_cast<Object**>(elm_addr);
                if (Heap::InNewSpace(*elm_p))
                  cross_gen_array_elements++;
              }
            } else {
              rset_marked_pointers++;
              if (Heap::InNewSpace(*obj))
                cross_gen_pointers++;
            }
          }
        }
      }
    }
  }

  pct = rset_marked_pointers == 0 ?
        0 : cross_gen_pointers * 100 / rset_marked_pointers;
  PrintF("  rset-marked pointers %d, to-new-space %d (%%%d)\n",
            rset_marked_pointers, cross_gen_pointers, pct);
  PrintF("  rset_marked arrays %d, ", rset_marked_arrays);
  PrintF("  elements %d, ", rset_marked_array_elements);
  pct = rset_marked_array_elements == 0 ? 0
           : cross_gen_array_elements * 100 / rset_marked_array_elements;
  PrintF("  pointers to new space %d (%%%d)\n", cross_gen_array_elements, pct);
  PrintF("  total rset-marked bits %d\n",
            (rset_marked_pointers + rset_marked_arrays));
  pct = (rset_marked_pointers + rset_marked_array_elements) == 0 ? 0
        : (cross_gen_pointers + cross_gen_array_elements) * 100 /
          (rset_marked_pointers + rset_marked_array_elements);
  PrintF("  total rset pointers %d, true cross generation ones %d (%%%d)\n",
         (rset_marked_pointers + rset_marked_array_elements),
         (cross_gen_pointers + cross_gen_array_elements),
         pct);

  ClearHistograms();
  HeapObjectIterator obj_it(this);
  while (obj_it.has_next()) { CollectHistogramInfo(obj_it.next()); }
  ReportHistogram(true);
}


// Dump the range of remembered set words between [start, end) corresponding
// to the pointers starting at object_p.  The allocation_top is an object
// pointer which should not be read past.  This is important for large object
// pages, where some bits in the remembered set range do not correspond to
// allocated addresses.
static void PrintRSetRange(Address start, Address end, Object** object_p,
                           Address allocation_top) {
  Address rset_address = start;

  // If the range starts on on odd numbered word (eg, for large object extra
  // remembered set ranges), print some spaces.
  if ((reinterpret_cast<uintptr_t>(start) / kIntSize) % 2 == 1) {
    PrintF("                                    ");
  }

  // Loop over all the words in the range.
  while (rset_address < end) {
    uint32_t rset_word = Memory::uint32_at(rset_address);
    int bit_position = 0;

    // Loop over all the bits in the word.
    while (bit_position < kBitsPerInt) {
      if (object_p == reinterpret_cast<Object**>(allocation_top)) {
        // Print a bar at the allocation pointer.
        PrintF("|");
      } else if (object_p > reinterpret_cast<Object**>(allocation_top)) {
        // Do not dereference object_p past the allocation pointer.
        PrintF("#");
      } else if ((rset_word & (1 << bit_position)) == 0) {
        // Print a dot for zero bits.
        PrintF(".");
      } else if (Heap::InNewSpace(*object_p)) {
        // Print an X for one bits for pointers to new space.
        PrintF("X");
      } else {
        // Print a circle for one bits for pointers to old space.
        PrintF("o");
      }

      // Print a space after every 8th bit except the last.
      if (bit_position % 8 == 7 && bit_position != (kBitsPerInt - 1)) {
        PrintF(" ");
      }

      // Advance to next bit.
      bit_position++;
      object_p++;
    }

    // Print a newline after every odd numbered word, otherwise a space.
    if ((reinterpret_cast<uintptr_t>(rset_address) / kIntSize) % 2 == 1) {
      PrintF("\n");
    } else {
      PrintF(" ");
    }

    // Advance to next remembered set word.
    rset_address += kIntSize;
  }
}


void PagedSpace::DoPrintRSet(const char* space_name) {
  PageIterator it(this, PageIterator::PAGES_IN_USE);
  while (it.has_next()) {
    Page* p = it.next();
    PrintF("%s page 0x%x:\n", space_name, p);
    PrintRSetRange(p->RSetStart(), p->RSetEnd(),
                   reinterpret_cast<Object**>(p->ObjectAreaStart()),
                   p->AllocationTop());
    PrintF("\n");
  }
}


void OldSpace::PrintRSet() { DoPrintRSet("old"); }
#endif

// -----------------------------------------------------------------------------
// FixedSpace implementation

void FixedSpace::PrepareForMarkCompact(bool will_compact) {
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
    computed_size += page_top - page->ObjectAreaStart();
    if (it.has_next()) {
      accounting_stats_.WasteBytes(page->ObjectAreaEnd() - page_top);
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

  // There is no next page in this space.  Try free list allocation.
  // The fixed space free list implicitly assumes that all free blocks
  // are of the fixed size.
  if (size_in_bytes == object_size_in_bytes_) {
    Object* result = free_list_.Allocate();
    if (!result->IsFailure()) {
      accounting_stats_.AllocateBytes(size_in_bytes);
      return HeapObject::cast(result);
    }
  }

  // Free list allocation failed and there is no next page.  Fail if we have
  // hit the old generation size limit that should cause a garbage
  // collection.
  if (!Heap::always_allocate() && Heap::OldGenerationAllocationLimitReached()) {
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
  ASSERT(current_page->ObjectAreaEnd() - allocation_info_.top == page_extra_);
  ASSERT_EQ(object_size_in_bytes_, size_in_bytes);
  accounting_stats_.WasteBytes(page_extra_);
  SetAllocationInfo(&allocation_info_, current_page->next_page());
  return AllocateLinearly(&allocation_info_, size_in_bytes);
}


#ifdef DEBUG
void FixedSpace::ReportStatistics() {
  int pct = Available() * 100 / Capacity();
  PrintF("  capacity: %d, waste: %d, available: %d, %%%d\n",
         Capacity(), Waste(), Available(), pct);

  // Report remembered set statistics.
  int rset_marked_pointers = 0;
  int cross_gen_pointers = 0;

  PageIterator page_it(this, PageIterator::PAGES_IN_USE);
  while (page_it.has_next()) {
    Page* p = page_it.next();

    for (Address rset_addr = p->RSetStart();
         rset_addr < p->RSetEnd();
         rset_addr += kIntSize) {
      int rset = Memory::int_at(rset_addr);
      if (rset != 0) {
        // Bits were set
        int intoff = rset_addr - p->address();
        int bitoff = 0;
        for (; bitoff < kBitsPerInt; ++bitoff) {
          if ((rset & (1 << bitoff)) != 0) {
            int bitpos = intoff*kBitsPerByte + bitoff;
            Address slot = p->OffsetToAddress(bitpos << kObjectAlignmentBits);
            Object** obj = reinterpret_cast<Object**>(slot);
            rset_marked_pointers++;
            if (Heap::InNewSpace(*obj))
              cross_gen_pointers++;
          }
        }
      }
    }
  }

  pct = rset_marked_pointers == 0 ?
          0 : cross_gen_pointers * 100 / rset_marked_pointers;
  PrintF("  rset-marked pointers %d, to-new-space %d (%%%d)\n",
            rset_marked_pointers, cross_gen_pointers, pct);

  ClearHistograms();
  HeapObjectIterator obj_it(this);
  while (obj_it.has_next()) { CollectHistogramInfo(obj_it.next()); }
  ReportHistogram(false);
}


void FixedSpace::PrintRSet() { DoPrintRSet(name_); }
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
         object->map() == Heap::two_pointer_filler_map());
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
  ASSERT(has_next());
  HeapObject* object = current_->GetObject();
  current_ = current_->next();
  return object;
}


// -----------------------------------------------------------------------------
// LargeObjectChunk

LargeObjectChunk* LargeObjectChunk::New(int size_in_bytes,
                                        size_t* chunk_size,
                                        Executability executable) {
  size_t requested = ChunkSizeFor(size_in_bytes);
  void* mem = MemoryAllocator::AllocateRawMemory(requested,
                                                 chunk_size,
                                                 executable);
  if (mem == NULL) return NULL;
  LOG(NewEvent("LargeObjectChunk", mem, *chunk_size));
  if (*chunk_size < requested) {
    MemoryAllocator::FreeRawMemory(mem, *chunk_size);
    LOG(DeleteEvent("LargeObjectChunk", mem));
    return NULL;
  }
  return reinterpret_cast<LargeObjectChunk*>(mem);
}


int LargeObjectChunk::ChunkSizeFor(int size_in_bytes) {
  int os_alignment = OS::AllocateAlignment();
  if (os_alignment < Page::kPageSize)
    size_in_bytes += (Page::kPageSize - os_alignment);
  return size_in_bytes + Page::kObjectStartOffset;
}

// -----------------------------------------------------------------------------
// LargeObjectSpace

LargeObjectSpace::LargeObjectSpace(AllocationSpace id)
    : Space(id, NOT_EXECUTABLE),  // Managed on a per-allocation basis
      first_chunk_(NULL),
      size_(0),
      page_count_(0) {}


bool LargeObjectSpace::Setup() {
  first_chunk_ = NULL;
  size_ = 0;
  page_count_ = 0;
  return true;
}


void LargeObjectSpace::TearDown() {
  while (first_chunk_ != NULL) {
    LargeObjectChunk* chunk = first_chunk_;
    first_chunk_ = first_chunk_->next();
    LOG(DeleteEvent("LargeObjectChunk", chunk->address()));
    MemoryAllocator::FreeRawMemory(chunk->address(), chunk->size());
  }

  size_ = 0;
  page_count_ = 0;
}


#ifdef ENABLE_HEAP_PROTECTION

void LargeObjectSpace::Protect() {
  LargeObjectChunk* chunk = first_chunk_;
  while (chunk != NULL) {
    MemoryAllocator::Protect(chunk->address(), chunk->size());
    chunk = chunk->next();
  }
}


void LargeObjectSpace::Unprotect() {
  LargeObjectChunk* chunk = first_chunk_;
  while (chunk != NULL) {
    bool is_code = chunk->GetObject()->IsCode();
    MemoryAllocator::Unprotect(chunk->address(), chunk->size(),
                               is_code ? EXECUTABLE : NOT_EXECUTABLE);
    chunk = chunk->next();
  }
}

#endif


Object* LargeObjectSpace::AllocateRawInternal(int requested_size,
                                              int object_size,
                                              Executability executable) {
  ASSERT(0 < object_size && object_size <= requested_size);

  // Check if we want to force a GC before growing the old space further.
  // If so, fail the allocation.
  if (!Heap::always_allocate() && Heap::OldGenerationAllocationLimitReached()) {
    return Failure::RetryAfterGC(requested_size, identity());
  }

  size_t chunk_size;
  LargeObjectChunk* chunk =
      LargeObjectChunk::New(requested_size, &chunk_size, executable);
  if (chunk == NULL) {
    return Failure::RetryAfterGC(requested_size, identity());
  }

  size_ += chunk_size;
  page_count_++;
  chunk->set_next(first_chunk_);
  chunk->set_size(chunk_size);
  first_chunk_ = chunk;

  // Set the object address and size in the page header and clear its
  // remembered set.
  Page* page = Page::FromAddress(RoundUp(chunk->address(), Page::kPageSize));
  Address object_address = page->ObjectAreaStart();
  // Clear the low order bit of the second word in the page to flag it as a
  // large object page.  If the chunk_size happened to be written there, its
  // low order bit should already be clear.
  ASSERT((chunk_size & 0x1) == 0);
  page->is_normal_page &= ~0x1;
  page->ClearRSet();
  int extra_bytes = requested_size - object_size;
  if (extra_bytes > 0) {
    // The extra memory for the remembered set should be cleared.
    memset(object_address + object_size, 0, extra_bytes);
  }

  return HeapObject::FromAddress(object_address);
}


Object* LargeObjectSpace::AllocateRawCode(int size_in_bytes) {
  ASSERT(0 < size_in_bytes);
  return AllocateRawInternal(size_in_bytes,
                             size_in_bytes,
                             EXECUTABLE);
}


Object* LargeObjectSpace::AllocateRawFixedArray(int size_in_bytes) {
  ASSERT(0 < size_in_bytes);
  int extra_rset_bytes = ExtraRSetBytesFor(size_in_bytes);
  return AllocateRawInternal(size_in_bytes + extra_rset_bytes,
                             size_in_bytes,
                             NOT_EXECUTABLE);
}


Object* LargeObjectSpace::AllocateRaw(int size_in_bytes) {
  ASSERT(0 < size_in_bytes);
  return AllocateRawInternal(size_in_bytes,
                             size_in_bytes,
                             NOT_EXECUTABLE);
}


// GC support
Object* LargeObjectSpace::FindObject(Address a) {
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


void LargeObjectSpace::ClearRSet() {
  ASSERT(Page::is_rset_in_use());

  LargeObjectIterator it(this);
  while (it.has_next()) {
    HeapObject* object = it.next();
    // We only have code, sequential strings, or fixed arrays in large
    // object space, and only fixed arrays need remembered set support.
    if (object->IsFixedArray()) {
      // Clear the normal remembered set region of the page;
      Page* page = Page::FromAddress(object->address());
      page->ClearRSet();

      // Clear the extra remembered set.
      int size = object->Size();
      int extra_rset_bytes = ExtraRSetBytesFor(size);
      memset(object->address() + size, 0, extra_rset_bytes);
    }
  }
}


void LargeObjectSpace::IterateRSet(ObjectSlotCallback copy_object_func) {
  ASSERT(Page::is_rset_in_use());

  static void* lo_rset_histogram = StatsTable::CreateHistogram(
      "V8.RSetLO",
      0,
      // Keeping this histogram's buckets the same as the paged space histogram.
      Page::kObjectAreaSize / kPointerSize,
      30);

  LargeObjectIterator it(this);
  while (it.has_next()) {
    // We only have code, sequential strings, or fixed arrays in large
    // object space, and only fixed arrays can possibly contain pointers to
    // the young generation.
    HeapObject* object = it.next();
    if (object->IsFixedArray()) {
      // Iterate the normal page remembered set range.
      Page* page = Page::FromAddress(object->address());
      Address object_end = object->address() + object->Size();
      int count = Heap::IterateRSetRange(page->ObjectAreaStart(),
                                         Min(page->ObjectAreaEnd(), object_end),
                                         page->RSetStart(),
                                         copy_object_func);

      // Iterate the extra array elements.
      if (object_end > page->ObjectAreaEnd()) {
        count += Heap::IterateRSetRange(page->ObjectAreaEnd(), object_end,
                                        object_end, copy_object_func);
      }
      if (lo_rset_histogram != NULL) {
        StatsTable::AddHistogramSample(lo_rset_histogram, count);
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
      MarkCompactCollector::tracer()->decrement_marked_count();
      previous = current;
      current = current->next();
    } else {
      Address chunk_address = current->address();
      size_t chunk_size = current->size();

      // Cut the chunk out from the chunk list.
      current = current->next();
      if (previous == NULL) {
        first_chunk_ = current;
      } else {
        previous->set_next(current);
      }

      // Free the chunk.
      if (object->IsCode()) {
        LOG(CodeDeleteEvent(object->address()));
      }
      size_ -= chunk_size;
      page_count_--;
      MemoryAllocator::FreeRawMemory(chunk_address, chunk_size);
      LOG(DeleteEvent("LargeObjectChunk", chunk_address));
    }
  }
}


bool LargeObjectSpace::Contains(HeapObject* object) {
  Address address = object->address();
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
    ASSERT(Heap::map_space()->Contains(map));

    // We have only code, sequential strings, fixed arrays, and byte arrays
    // in large object space.
    ASSERT(object->IsCode() || object->IsSeqString()
           || object->IsFixedArray() || object->IsByteArray());

    // The object itself should look OK.
    object->Verify();

    // Byte arrays and strings don't have interior pointers.
    if (object->IsCode()) {
      VerifyPointersVisitor code_visitor;
      Code::cast(object)->ConvertICTargetsFromAddressToObject();
      object->IterateBody(map->instance_type(),
                          object->Size(),
                          &code_visitor);
      Code::cast(object)->ConvertICTargetsFromObjectToAddress();
    } else if (object->IsFixedArray()) {
      // We loop over fixed arrays ourselves, rather then using the visitor,
      // because the visitor doesn't support the start/offset iteration
      // needed for IsRSetSet.
      FixedArray* array = FixedArray::cast(object);
      for (int j = 0; j < array->length(); j++) {
        Object* element = array->get(j);
        if (element->IsHeapObject()) {
          HeapObject* element_object = HeapObject::cast(element);
          ASSERT(Heap::Contains(element_object));
          ASSERT(element_object->map()->IsMap());
          if (Heap::InNewSpace(element_object)) {
            ASSERT(Page::IsRSetSet(object->address(),
                                   FixedArray::kHeaderSize + j * kPointerSize));
          }
        }
      }
    }
  }
}


void LargeObjectSpace::Print() {
  LargeObjectIterator it(this);
  while (it.has_next()) {
    it.next()->Print();
  }
}


void LargeObjectSpace::ReportStatistics() {
  PrintF("  size: %d\n", size_);
  int num_objects = 0;
  ClearHistograms();
  LargeObjectIterator it(this);
  while (it.has_next()) {
    num_objects++;
    CollectHistogramInfo(it.next());
  }

  PrintF("  number of objects %d\n", num_objects);
  if (num_objects > 0) ReportHistogram(false);
}


void LargeObjectSpace::CollectCodeStatistics() {
  LargeObjectIterator obj_it(this);
  while (obj_it.has_next()) {
    HeapObject* obj = obj_it.next();
    if (obj->IsCode()) {
      Code* code = Code::cast(obj);
      code_kind_statistics[code->kind()] += code->Size();
    }
  }
}


void LargeObjectSpace::PrintRSet() {
  LargeObjectIterator it(this);
  while (it.has_next()) {
    HeapObject* object = it.next();
    if (object->IsFixedArray()) {
      Page* page = Page::FromAddress(object->address());

      Address allocation_top = object->address() + object->Size();
      PrintF("large page 0x%x:\n", page);
      PrintRSetRange(page->RSetStart(), page->RSetEnd(),
                     reinterpret_cast<Object**>(object->address()),
                     allocation_top);
      int extra_array_bytes = object->Size() - Page::kObjectAreaSize;
      int extra_rset_bits = RoundUp(extra_array_bytes / kPointerSize,
                                    kBitsPerInt);
      PrintF("------------------------------------------------------------"
             "-----------\n");
      PrintRSetRange(allocation_top,
                     allocation_top + extra_rset_bits / kBitsPerByte,
                     reinterpret_cast<Object**>(object->address()
                                                + Page::kObjectAreaSize),
                     allocation_top);
      PrintF("\n");
    }
  }
}
#endif  // DEBUG

} }  // namespace v8::internal
