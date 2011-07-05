// Copyright 2006-2010 the V8 project authors. All rights reserved.
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

#ifndef V8_SPACES_INL_H_
#define V8_SPACES_INL_H_

#include "memory.h"
#include "spaces.h"

namespace v8 {
namespace internal {


// -----------------------------------------------------------------------------
// PageIterator

bool PageIterator::has_next() {
  return prev_page_ != stop_page_;
}


Page* PageIterator::next() {
  ASSERT(has_next());
  prev_page_ = (prev_page_ == NULL)
               ? space_->first_page_
               : prev_page_->next_page();
  return prev_page_;
}


// -----------------------------------------------------------------------------
// Page

Page* Page::next_page() {
  return MemoryAllocator::GetNextPage(this);
}


Address Page::AllocationTop() {
  PagedSpace* owner = MemoryAllocator::PageOwner(this);
  return owner->PageAllocationTop(this);
}


Address Page::AllocationWatermark() {
  PagedSpace* owner = MemoryAllocator::PageOwner(this);
  if (this == owner->AllocationTopPage()) {
    return owner->top();
  }
  return address() + AllocationWatermarkOffset();
}


uint32_t Page::AllocationWatermarkOffset() {
  return static_cast<uint32_t>((flags_ & kAllocationWatermarkOffsetMask) >>
                               kAllocationWatermarkOffsetShift);
}


void Page::SetAllocationWatermark(Address allocation_watermark) {
  if ((Heap::gc_state() == Heap::SCAVENGE) && IsWatermarkValid()) {
    // When iterating intergenerational references during scavenge
    // we might decide to promote an encountered young object.
    // We will allocate a space for such an object and put it
    // into the promotion queue to process it later.
    // If space for object was allocated somewhere beyond allocation
    // watermark this might cause garbage pointers to appear under allocation
    // watermark. To avoid visiting them during dirty regions iteration
    // which might be still in progress we store a valid allocation watermark
    // value and mark this page as having an invalid watermark.
    SetCachedAllocationWatermark(AllocationWatermark());
    InvalidateWatermark(true);
  }

  flags_ = (flags_ & kFlagsMask) |
           Offset(allocation_watermark) << kAllocationWatermarkOffsetShift;
  ASSERT(AllocationWatermarkOffset()
         == static_cast<uint32_t>(Offset(allocation_watermark)));
}


void Page::SetCachedAllocationWatermark(Address allocation_watermark) {
  mc_first_forwarded = allocation_watermark;
}


Address Page::CachedAllocationWatermark() {
  return mc_first_forwarded;
}


uint32_t Page::GetRegionMarks() {
  return dirty_regions_;
}


void Page::SetRegionMarks(uint32_t marks) {
  dirty_regions_ = marks;
}


int Page::GetRegionNumberForAddress(Address addr) {
  // Each page is divided into 256 byte regions. Each region has a corresponding
  // dirty mark bit in the page header. Region can contain intergenerational
  // references iff its dirty mark is set.
  // A normal 8K page contains exactly 32 regions so all region marks fit
  // into 32-bit integer field. To calculate a region number we just divide
  // offset inside page by region size.
  // A large page can contain more then 32 regions. But we want to avoid
  // additional write barrier code for distinguishing between large and normal
  // pages so we just ignore the fact that addr points into a large page and
  // calculate region number as if addr pointed into a normal 8K page. This way
  // we get a region number modulo 32 so for large pages several regions might
  // be mapped to a single dirty mark.
  ASSERT_PAGE_ALIGNED(this->address());
  STATIC_ASSERT((kPageAlignmentMask >> kRegionSizeLog2) < kBitsPerInt);

  // We are using masking with kPageAlignmentMask instead of Page::Offset()
  // to get an offset to the beginning of 8K page containing addr not to the
  // beginning of actual page which can be bigger then 8K.
  intptr_t offset_inside_normal_page = OffsetFrom(addr) & kPageAlignmentMask;
  return static_cast<int>(offset_inside_normal_page >> kRegionSizeLog2);
}


uint32_t Page::GetRegionMaskForAddress(Address addr) {
  return 1 << GetRegionNumberForAddress(addr);
}


uint32_t Page::GetRegionMaskForSpan(Address start, int length_in_bytes) {
  uint32_t result = 0;
  if (length_in_bytes >= kPageSize) {
    result = kAllRegionsDirtyMarks;
  } else if (length_in_bytes > 0) {
    int start_region = GetRegionNumberForAddress(start);
    int end_region =
        GetRegionNumberForAddress(start + length_in_bytes - kPointerSize);
    uint32_t start_mask = (~0) << start_region;
    uint32_t end_mask = ~((~1) << end_region);
    result = start_mask & end_mask;
    // if end_region < start_region, the mask is ored.
    if (result == 0) result = start_mask | end_mask;
  }
#ifdef DEBUG
  if (FLAG_enable_slow_asserts) {
    uint32_t expected = 0;
    for (Address a = start; a < start + length_in_bytes; a += kPointerSize) {
      expected |= GetRegionMaskForAddress(a);
    }
    ASSERT(expected == result);
  }
#endif
  return result;
}


void Page::MarkRegionDirty(Address address) {
  SetRegionMarks(GetRegionMarks() | GetRegionMaskForAddress(address));
}


bool Page::IsRegionDirty(Address address) {
  return GetRegionMarks() & GetRegionMaskForAddress(address);
}


void Page::ClearRegionMarks(Address start, Address end, bool reaches_limit) {
  int rstart = GetRegionNumberForAddress(start);
  int rend = GetRegionNumberForAddress(end);

  if (reaches_limit) {
    end += 1;
  }

  if ((rend - rstart) == 0) {
    return;
  }

  uint32_t bitmask = 0;

  if ((OffsetFrom(start) & kRegionAlignmentMask) == 0
      || (start == ObjectAreaStart())) {
    // First region is fully covered
    bitmask = 1 << rstart;
  }

  while (++rstart < rend) {
    bitmask |= 1 << rstart;
  }

  if (bitmask) {
    SetRegionMarks(GetRegionMarks() & ~bitmask);
  }
}


void Page::FlipMeaningOfInvalidatedWatermarkFlag() {
  watermark_invalidated_mark_ ^= 1 << WATERMARK_INVALIDATED;
}


bool Page::IsWatermarkValid() {
  return (flags_ & (1 << WATERMARK_INVALIDATED)) != watermark_invalidated_mark_;
}


void Page::InvalidateWatermark(bool value) {
  if (value) {
    flags_ = (flags_ & ~(1 << WATERMARK_INVALIDATED)) |
             watermark_invalidated_mark_;
  } else {
    flags_ = (flags_ & ~(1 << WATERMARK_INVALIDATED)) |
             (watermark_invalidated_mark_ ^ (1 << WATERMARK_INVALIDATED));
  }

  ASSERT(IsWatermarkValid() == !value);
}


bool Page::GetPageFlag(PageFlag flag) {
  return (flags_ & static_cast<intptr_t>(1 << flag)) != 0;
}


void Page::SetPageFlag(PageFlag flag, bool value) {
  if (value) {
    flags_ |= static_cast<intptr_t>(1 << flag);
  } else {
    flags_ &= ~static_cast<intptr_t>(1 << flag);
  }
}


void Page::ClearPageFlags() {
  flags_ = 0;
}


void Page::ClearGCFields() {
  InvalidateWatermark(true);
  SetAllocationWatermark(ObjectAreaStart());
  if (Heap::gc_state() == Heap::SCAVENGE) {
    SetCachedAllocationWatermark(ObjectAreaStart());
  }
  SetRegionMarks(kAllRegionsCleanMarks);
}


bool Page::WasInUseBeforeMC() {
  return GetPageFlag(WAS_IN_USE_BEFORE_MC);
}


void Page::SetWasInUseBeforeMC(bool was_in_use) {
  SetPageFlag(WAS_IN_USE_BEFORE_MC, was_in_use);
}


bool Page::IsLargeObjectPage() {
  return !GetPageFlag(IS_NORMAL_PAGE);
}


void Page::SetIsLargeObjectPage(bool is_large_object_page) {
  SetPageFlag(IS_NORMAL_PAGE, !is_large_object_page);
}

bool Page::IsPageExecutable() {
  return GetPageFlag(IS_EXECUTABLE);
}


void Page::SetIsPageExecutable(bool is_page_executable) {
  SetPageFlag(IS_EXECUTABLE, is_page_executable);
}


// -----------------------------------------------------------------------------
// MemoryAllocator

void MemoryAllocator::ChunkInfo::init(Address a, size_t s, PagedSpace* o) {
  address_ = a;
  size_ = s;
  owner_ = o;
  executable_ = (o == NULL) ? NOT_EXECUTABLE : o->executable();
}


bool MemoryAllocator::IsValidChunk(int chunk_id) {
  if (!IsValidChunkId(chunk_id)) return false;

  ChunkInfo& c = chunks_[chunk_id];
  return (c.address() != NULL) && (c.size() != 0) && (c.owner() != NULL);
}


bool MemoryAllocator::IsValidChunkId(int chunk_id) {
  return (0 <= chunk_id) && (chunk_id < max_nof_chunks_);
}


bool MemoryAllocator::IsPageInSpace(Page* p, PagedSpace* space) {
  ASSERT(p->is_valid());

  int chunk_id = GetChunkId(p);
  if (!IsValidChunkId(chunk_id)) return false;

  ChunkInfo& c = chunks_[chunk_id];
  return (c.address() <= p->address()) &&
         (p->address() < c.address() + c.size()) &&
         (space == c.owner());
}


Page* MemoryAllocator::GetNextPage(Page* p) {
  ASSERT(p->is_valid());
  intptr_t raw_addr = p->opaque_header & ~Page::kPageAlignmentMask;
  return Page::FromAddress(AddressFrom<Address>(raw_addr));
}


int MemoryAllocator::GetChunkId(Page* p) {
  ASSERT(p->is_valid());
  return static_cast<int>(p->opaque_header & Page::kPageAlignmentMask);
}


void MemoryAllocator::SetNextPage(Page* prev, Page* next) {
  ASSERT(prev->is_valid());
  int chunk_id = GetChunkId(prev);
  ASSERT_PAGE_ALIGNED(next->address());
  prev->opaque_header = OffsetFrom(next->address()) | chunk_id;
}


PagedSpace* MemoryAllocator::PageOwner(Page* page) {
  int chunk_id = GetChunkId(page);
  ASSERT(IsValidChunk(chunk_id));
  return chunks_[chunk_id].owner();
}


bool MemoryAllocator::InInitialChunk(Address address) {
  if (initial_chunk_ == NULL) return false;

  Address start = static_cast<Address>(initial_chunk_->address());
  return (start <= address) && (address < start + initial_chunk_->size());
}


#ifdef ENABLE_HEAP_PROTECTION

void MemoryAllocator::Protect(Address start, size_t size) {
  OS::Protect(start, size);
}


void MemoryAllocator::Unprotect(Address start,
                                size_t size,
                                Executability executable) {
  OS::Unprotect(start, size, executable);
}


void MemoryAllocator::ProtectChunkFromPage(Page* page) {
  int id = GetChunkId(page);
  OS::Protect(chunks_[id].address(), chunks_[id].size());
}


void MemoryAllocator::UnprotectChunkFromPage(Page* page) {
  int id = GetChunkId(page);
  OS::Unprotect(chunks_[id].address(), chunks_[id].size(),
                chunks_[id].owner()->executable() == EXECUTABLE);
}

#endif


// --------------------------------------------------------------------------
// PagedSpace

bool PagedSpace::Contains(Address addr) {
  Page* p = Page::FromAddress(addr);
  if (!p->is_valid()) return false;
  return MemoryAllocator::IsPageInSpace(p, this);
}


bool PagedSpace::SafeContains(Address addr) {
  if (!MemoryAllocator::SafeIsInAPageChunk(addr)) return false;
  Page* p = Page::FromAddress(addr);
  if (!p->is_valid()) return false;
  return MemoryAllocator::IsPageInSpace(p, this);
}


// Try linear allocation in the page of alloc_info's allocation top.  Does
// not contain slow case logic (eg, move to the next page or try free list
// allocation) so it can be used by all the allocation functions and for all
// the paged spaces.
HeapObject* PagedSpace::AllocateLinearly(AllocationInfo* alloc_info,
                                         int size_in_bytes) {
  Address current_top = alloc_info->top;
  Address new_top = current_top + size_in_bytes;
  if (new_top > alloc_info->limit) return NULL;

  alloc_info->top = new_top;
  ASSERT(alloc_info->VerifyPagedAllocation());
  accounting_stats_.AllocateBytes(size_in_bytes);
  return HeapObject::FromAddress(current_top);
}


// Raw allocation.
MaybeObject* PagedSpace::AllocateRaw(int size_in_bytes) {
  ASSERT(HasBeenSetup());
  ASSERT_OBJECT_SIZE(size_in_bytes);
  HeapObject* object = AllocateLinearly(&allocation_info_, size_in_bytes);
  if (object != NULL) return object;

  object = SlowAllocateRaw(size_in_bytes);
  if (object != NULL) return object;

  return Failure::RetryAfterGC(identity());
}


// Reallocating (and promoting) objects during a compacting collection.
MaybeObject* PagedSpace::MCAllocateRaw(int size_in_bytes) {
  ASSERT(HasBeenSetup());
  ASSERT_OBJECT_SIZE(size_in_bytes);
  HeapObject* object = AllocateLinearly(&mc_forwarding_info_, size_in_bytes);
  if (object != NULL) return object;

  object = SlowMCAllocateRaw(size_in_bytes);
  if (object != NULL) return object;

  return Failure::RetryAfterGC(identity());
}


// -----------------------------------------------------------------------------
// LargeObjectChunk

Address LargeObjectChunk::GetStartAddress() {
  // Round the chunk address up to the nearest page-aligned address
  // and return the heap object in that page.
  Page* page = Page::FromAddress(RoundUp(address(), Page::kPageSize));
  return page->ObjectAreaStart();
}


void LargeObjectChunk::Free(Executability executable) {
  MemoryAllocator::FreeRawMemory(address(), size(), executable);
}

// -----------------------------------------------------------------------------
// NewSpace

MaybeObject* NewSpace::AllocateRawInternal(int size_in_bytes,
                                           AllocationInfo* alloc_info) {
  Address new_top = alloc_info->top + size_in_bytes;
  if (new_top > alloc_info->limit) return Failure::RetryAfterGC();

  Object* obj = HeapObject::FromAddress(alloc_info->top);
  alloc_info->top = new_top;
#ifdef DEBUG
  SemiSpace* space =
      (alloc_info == &allocation_info_) ? &to_space_ : &from_space_;
  ASSERT(space->low() <= alloc_info->top
         && alloc_info->top <= space->high()
         && alloc_info->limit == space->high());
#endif
  return obj;
}


template <typename StringType>
void NewSpace::ShrinkStringAtAllocationBoundary(String* string, int length) {
  ASSERT(length <= string->length());
  ASSERT(string->IsSeqString());
  ASSERT(string->address() + StringType::SizeFor(string->length()) ==
         allocation_info_.top);
  allocation_info_.top =
      string->address() + StringType::SizeFor(length);
  string->set_length(length);
}


bool FreeListNode::IsFreeListNode(HeapObject* object) {
  return object->map() == Heap::raw_unchecked_byte_array_map()
      || object->map() == Heap::raw_unchecked_one_pointer_filler_map()
      || object->map() == Heap::raw_unchecked_two_pointer_filler_map();
}

} }  // namespace v8::internal

#endif  // V8_SPACES_INL_H_
