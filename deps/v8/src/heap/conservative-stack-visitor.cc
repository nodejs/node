// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/conservative-stack-visitor.h"

#include "src/execution/isolate-inl.h"
#include "src/objects/visitors.h"

#ifdef V8_COMPRESS_POINTERS
#include "src/common/ptr-compr-inl.h"
#endif  // V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

ConservativeStackVisitor::ConservativeStackVisitor(Isolate* isolate,
                                                   RootVisitor* delegate)
    : cage_base_(isolate),
      delegate_(delegate),
      allocator_(isolate->heap()->memory_allocator()),
      collector_(delegate->collector()) {}

namespace {

// This utility function returns the highest address in the page that is lower
// than maybe_inner_ptr, has its markbit set, and whose previous address (if it
// exists) does not have its markbit set. This address is guaranteed to be the
// start of a valid object in the page. In case the markbit corresponding to
// maybe_inner_ptr is set, the function bails out and returns kNullAddress.
Address FindPreviousObjectForConservativeMarking(const Page* page,
                                                 Address maybe_inner_ptr) {
  auto* bitmap = page->marking_bitmap<AccessMode::NON_ATOMIC>();
  const MarkBit::CellType* cells = bitmap->cells();

  // The first actual bit of the bitmap, corresponding to page->area_start(),
  // is at start_index which is somewhere in (not necessarily at the start of)
  // start_cell_index.
  const uint32_t start_index = page->AddressToMarkbitIndex(page->area_start());
  const uint32_t start_cell_index = Bitmap::IndexToCell(start_index);
  // We assume that all markbits before start_index are clear:
  // SLOW_DCHECK(bitmap->AllBitsClearInRange(0, start_index));
  // This has already been checked for the entire bitmap before starting marking
  // by MarkCompactCollector::VerifyMarkbitsAreClean.

  const uint32_t index = page->AddressToMarkbitIndex(maybe_inner_ptr);
  uint32_t cell_index = Bitmap::IndexToCell(index);
  const MarkBit::CellType mask = 1u << Bitmap::IndexInCell(index);
  MarkBit::CellType cell = cells[cell_index];

  // If the markbit is already set, bail out.
  if ((cell & mask) != 0) return kNullAddress;

  // Clear the bits corresponding to higher addresses in the cell.
  cell &= ((~static_cast<MarkBit::CellType>(0)) >>
           (Bitmap::kBitsPerCell - Bitmap::IndexInCell(index) - 1));

  // Traverse the bitmap backwards, until we find a markbit that is set and
  // whose previous markbit (if it exists) is unset.
  // First, iterate backwards to find a cell with any set markbit.
  while (cell == 0 && cell_index > start_cell_index) cell = cells[--cell_index];
  if (cell == 0) {
    DCHECK_EQ(start_cell_index, cell_index);
    // We have reached the start of the page.
    return page->area_start();
  }

  // We have found such a cell.
  const uint32_t leading_zeros = base::bits::CountLeadingZeros(cell);
  const uint32_t leftmost_ones =
      base::bits::CountLeadingZeros(~(cell << leading_zeros));
  const uint32_t index_of_last_leftmost_one =
      Bitmap::kBitsPerCell - leading_zeros - leftmost_ones;

  // If the leftmost sequence of set bits does not reach the start of the cell,
  // we found it.
  if (index_of_last_leftmost_one > 0) {
    return page->MarkbitIndexToAddress(cell_index * Bitmap::kBitsPerCell +
                                       index_of_last_leftmost_one);
  }

  // The leftmost sequence of set bits reaches the start of the cell. We must
  // keep traversing backwards until we find the first unset markbit.
  if (cell_index == start_cell_index) {
    // We have reached the start of the page.
    return page->area_start();
  }

  // Iterate backwards to find a cell with any unset markbit.
  do {
    cell = cells[--cell_index];
  } while (~cell == 0 && cell_index > start_cell_index);
  if (~cell == 0) {
    DCHECK_EQ(start_cell_index, cell_index);
    // We have reached the start of the page.
    return page->area_start();
  }

  // We have found such a cell.
  const uint32_t leading_ones = base::bits::CountLeadingZeros(~cell);
  const uint32_t index_of_last_leading_one =
      Bitmap::kBitsPerCell - leading_ones;
  DCHECK_LT(0, index_of_last_leading_one);
  return page->MarkbitIndexToAddress(cell_index * Bitmap::kBitsPerCell +
                                     index_of_last_leading_one);
}

}  // namespace

// static
Address ConservativeStackVisitor::FindBasePtrForMarking(
    Address maybe_inner_ptr, MemoryAllocator* allocator,
    GarbageCollector collector) {
  // Check if the pointer is contained by a normal or large page owned by this
  // heap. Bail out if it is not.
  const MemoryChunk* chunk =
      allocator->LookupChunkContainingAddress(maybe_inner_ptr);
  if (chunk == nullptr) return kNullAddress;
  DCHECK(chunk->Contains(maybe_inner_ptr));
  // If it is contained in a large page, we want to mark the only object on it.
  if (chunk->IsLargePage()) {
    // This could be simplified if we could guarantee that there are no free
    // space or filler objects in large pages. A few cctests violate this now.
    HeapObject obj(static_cast<const LargePage*>(chunk)->GetObject());
    PtrComprCageBase cage_base{chunk->heap()->isolate()};
    return obj.IsFreeSpaceOrFiller(cage_base) ? kNullAddress : obj.address();
  }
  // Otherwise, we have a pointer inside a normal page.
  const Page* page = static_cast<const Page*>(chunk);
  // If it is not in the young generation and we're only interested in young
  // generation pointers, we must ignore it.
  if (Heap::IsYoungGenerationCollector(collector) && !page->InYoungGeneration())
    return kNullAddress;
  // If it is in the young generation "from" semispace, it is not used and we
  // must ignore it, as its markbits may not be clean.
  if (page->IsFromPage()) return kNullAddress;
  // Try to find the address of a previous valid object on this page.
  Address base_ptr =
      FindPreviousObjectForConservativeMarking(page, maybe_inner_ptr);
  // If the markbit is set, then we have an object that does not need to be
  // marked.
  if (base_ptr == kNullAddress) return kNullAddress;
  // Iterate through the objects in the page forwards, until we find the object
  // containing maybe_inner_ptr.
  DCHECK_LE(base_ptr, maybe_inner_ptr);
  PtrComprCageBase cage_base{page->heap()->isolate()};
  while (true) {
    HeapObject obj(HeapObject::FromAddress(base_ptr));
    const int size = obj.Size(cage_base);
    DCHECK_LT(0, size);
    if (maybe_inner_ptr < base_ptr + size)
      return obj.IsFreeSpaceOrFiller(cage_base) ? kNullAddress : base_ptr;
    base_ptr += size;
    DCHECK_LT(base_ptr, page->area_end());
  }
}

void ConservativeStackVisitor::VisitPointer(const void* pointer) {
  auto address = reinterpret_cast<Address>(const_cast<void*>(pointer));
  VisitConservativelyIfPointer(address);
#ifdef V8_COMPRESS_POINTERS
  V8HeapCompressionScheme::ProcessIntermediatePointers(
      cage_base_, address,
      [this](Address ptr) { VisitConservativelyIfPointer(ptr); });
#endif  // V8_COMPRESS_POINTERS
}

void ConservativeStackVisitor::VisitConservativelyIfPointer(Address address) {
  Address base_ptr = FindBasePtrForMarking(address, allocator_, collector_);
  if (base_ptr == kNullAddress) return;
  HeapObject obj = HeapObject::FromAddress(base_ptr);
  Object root = obj;
  delegate_->VisitRootPointer(Root::kHandleScope, nullptr,
                              FullObjectSlot(&root));
  // Check that the delegate visitor did not modify the root slot.
  DCHECK_EQ(root, obj);
}

}  // namespace internal
}  // namespace v8
