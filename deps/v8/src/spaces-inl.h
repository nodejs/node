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

#ifndef V8_SPACES_INL_H_
#define V8_SPACES_INL_H_

#include "memory.h"
#include "spaces.h"

namespace v8 { namespace internal {


// -----------------------------------------------------------------------------
// HeapObjectIterator

bool HeapObjectIterator::has_next() {
  if (cur_addr_ < cur_limit_) {
    return true;  // common case
  }
  ASSERT(cur_addr_ == cur_limit_);
  return HasNextInNextPage();  // slow path
}


HeapObject* HeapObjectIterator::next() {
  ASSERT(has_next());

  HeapObject* obj = HeapObject::FromAddress(cur_addr_);
  int obj_size = (size_func_ == NULL) ? obj->Size() : size_func_(obj);
  ASSERT_OBJECT_SIZE(obj_size);

  cur_addr_ += obj_size;
  ASSERT(cur_addr_ <= cur_limit_);

  return obj;
}


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


void Page::ClearRSet() {
  // This method can be called in all rset states.
  memset(RSetStart(), 0, kRSetEndOffset - kRSetStartOffset);
}


// Give an address a (32-bits):
// | page address | words (6) | bit offset (5) | pointer alignment (2) |
// The rset address is computed as:
//    page_address + words * 4

Address Page::ComputeRSetBitPosition(Address address, int offset,
                                     uint32_t* bitmask) {
  ASSERT(Page::is_rset_in_use());

  Page* page = Page::FromAddress(address);
  uint32_t bit_offset = ArithmeticShiftRight(page->Offset(address) + offset,
                                             kObjectAlignmentBits);
  *bitmask = 1 << (bit_offset % kBitsPerInt);

  Address rset_address =
      page->address() + (bit_offset / kBitsPerInt) * kIntSize;
  // The remembered set address is either in the normal remembered set range
  // of a page or else we have a large object page.
  ASSERT((page->RSetStart() <= rset_address && rset_address < page->RSetEnd())
         || page->IsLargeObjectPage());

  if (rset_address >= page->RSetEnd()) {
    // We have a large object page, and the remembered set address is actually
    // past the end of the object.  The address of the remembered set in this
    // case is the extra remembered set start address at the address of the
    // end of the object:
    //   (page->ObjectAreaStart() + object size)
    // plus the offset of the computed remembered set address from the start
    // of the object:
    //   (rset_address - page->ObjectAreaStart()).
    // Ie, we can just add the object size.
    ASSERT(HeapObject::FromAddress(address)->IsFixedArray());
    rset_address +=
        FixedArray::SizeFor(Memory::int_at(page->ObjectAreaStart()
                                           + Array::kLengthOffset));
  }
  return rset_address;
}


void Page::SetRSet(Address address, int offset) {
  uint32_t bitmask = 0;
  Address rset_address = ComputeRSetBitPosition(address, offset, &bitmask);
  Memory::uint32_at(rset_address) |= bitmask;

  ASSERT(IsRSetSet(address, offset));
}


// Clears the corresponding remembered set bit for a given address.
void Page::UnsetRSet(Address address, int offset) {
  uint32_t bitmask = 0;
  Address rset_address = ComputeRSetBitPosition(address, offset, &bitmask);
  Memory::uint32_at(rset_address) &= ~bitmask;

  ASSERT(!IsRSetSet(address, offset));
}


bool Page::IsRSetSet(Address address, int offset) {
  uint32_t bitmask = 0;
  Address rset_address = ComputeRSetBitPosition(address, offset, &bitmask);
  return (Memory::uint32_at(rset_address) & bitmask) != 0;
}


// -----------------------------------------------------------------------------
// MemoryAllocator

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
  int raw_addr = p->opaque_header & ~Page::kPageAlignmentMask;
  return Page::FromAddress(AddressFrom<Address>(raw_addr));
}


int MemoryAllocator::GetChunkId(Page* p) {
  ASSERT(p->is_valid());
  return p->opaque_header & Page::kPageAlignmentMask;
}


void MemoryAllocator::SetNextPage(Page* prev, Page* next) {
  ASSERT(prev->is_valid());
  int chunk_id = prev->opaque_header & Page::kPageAlignmentMask;
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
  ASSERT(p->is_valid());

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
Object* PagedSpace::AllocateRaw(int size_in_bytes) {
  ASSERT(HasBeenSetup());
  ASSERT_OBJECT_SIZE(size_in_bytes);
  HeapObject* object = AllocateLinearly(&allocation_info_, size_in_bytes);
  if (object != NULL) return object;

  object = SlowAllocateRaw(size_in_bytes);
  if (object != NULL) return object;

  return Failure::RetryAfterGC(size_in_bytes, identity());
}


// Reallocating (and promoting) objects during a compacting collection.
Object* PagedSpace::MCAllocateRaw(int size_in_bytes) {
  ASSERT(HasBeenSetup());
  ASSERT_OBJECT_SIZE(size_in_bytes);
  HeapObject* object = AllocateLinearly(&mc_forwarding_info_, size_in_bytes);
  if (object != NULL) return object;

  object = SlowMCAllocateRaw(size_in_bytes);
  if (object != NULL) return object;

  return Failure::RetryAfterGC(size_in_bytes, identity());
}


// -----------------------------------------------------------------------------
// LargeObjectChunk

HeapObject* LargeObjectChunk::GetObject() {
  // Round the chunk address up to the nearest page-aligned address
  // and return the heap object in that page.
  Page* page = Page::FromAddress(RoundUp(address(), Page::kPageSize));
  return HeapObject::FromAddress(page->ObjectAreaStart());
}


// -----------------------------------------------------------------------------
// LargeObjectSpace

int LargeObjectSpace::ExtraRSetBytesFor(int object_size) {
  int extra_rset_bits =
      RoundUp((object_size - Page::kObjectAreaSize) / kPointerSize,
              kBitsPerInt);
  return extra_rset_bits / kBitsPerByte;
}


Object* NewSpace::AllocateRawInternal(int size_in_bytes,
                                      AllocationInfo* alloc_info) {
  Address new_top = alloc_info->top + size_in_bytes;
  if (new_top > alloc_info->limit) return Failure::RetryAfterGC(size_in_bytes);

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

} }  // namespace v8::internal

#endif  // V8_SPACES_INL_H_
