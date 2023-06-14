// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/conservative-stack-visitor.h"

#include "src/execution/isolate-inl.h"
#include "src/heap/marking-inl.h"
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
  Address base_ptr = MarkingBitmap::FindPreviousObjectForConservativeMarking(
      page, maybe_inner_ptr);
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
  delegate_->VisitRootPointer(Root::kStackRoots, nullptr,
                              FullObjectSlot(&root));
  // Check that the delegate visitor did not modify the root slot.
  DCHECK_EQ(root, obj);
}

}  // namespace internal
}  // namespace v8
