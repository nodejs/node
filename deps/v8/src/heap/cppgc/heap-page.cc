// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap-page.h"

#include <algorithm>

#include "include/cppgc/internal/api-constants.h"
#include "src/base/logging.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header-inl.h"
#include "src/heap/cppgc/heap-space.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/object-start-bitmap-inl.h"
#include "src/heap/cppgc/object-start-bitmap.h"
#include "src/heap/cppgc/page-memory.h"
#include "src/heap/cppgc/raw-heap.h"

namespace cppgc {
namespace internal {

namespace {

Address AlignAddress(Address address, size_t alignment) {
  return reinterpret_cast<Address>(
      RoundUp(reinterpret_cast<uintptr_t>(address), alignment));
}

}  // namespace

STATIC_ASSERT(kPageSize == api_constants::kPageAlignment);

// static
BasePage* BasePage::FromPayload(void* payload) {
  return reinterpret_cast<BasePage*>(
      (reinterpret_cast<uintptr_t>(payload) & kPageBaseMask) + kGuardPageSize);
}

// static
const BasePage* BasePage::FromPayload(const void* payload) {
  return reinterpret_cast<const BasePage*>(
      (reinterpret_cast<uintptr_t>(const_cast<void*>(payload)) &
       kPageBaseMask) +
      kGuardPageSize);
}

HeapObjectHeader* BasePage::ObjectHeaderFromInnerAddress(void* address) {
  return const_cast<HeapObjectHeader*>(
      ObjectHeaderFromInnerAddress(const_cast<const void*>(address)));
}

const HeapObjectHeader* BasePage::ObjectHeaderFromInnerAddress(
    const void* address) {
  if (is_large()) {
    return LargePage::From(this)->ObjectHeader();
  }
  ObjectStartBitmap& bitmap = NormalPage::From(this)->object_start_bitmap();
  HeapObjectHeader* header =
      bitmap.FindHeader(static_cast<ConstAddress>(address));
  DCHECK_LT(address,
            reinterpret_cast<ConstAddress>(header) +
                header->GetSize<HeapObjectHeader::AccessMode::kAtomic>());
  DCHECK_NE(kFreeListGCInfoIndex,
            header->GetGCInfoIndex<HeapObjectHeader::AccessMode::kAtomic>());
  return header;
}

BasePage::BasePage(Heap* heap, BaseSpace* space, PageType type)
    : heap_(heap), space_(space), type_(type) {
  DCHECK_EQ(0u, (reinterpret_cast<uintptr_t>(this) - kGuardPageSize) &
                    kPageOffsetMask);
  DCHECK_EQ(reinterpret_cast<void*>(&heap_),
            FromPayload(this) + api_constants::kHeapOffset);
  DCHECK_EQ(&heap_->raw_heap(), space_->raw_heap());
}

// static
NormalPage* NormalPage::Create(NormalPageSpace* space) {
  DCHECK(space);
  Heap* heap = space->raw_heap()->heap();
  DCHECK(heap);
  void* memory = heap->page_backend()->AllocateNormalPageMemory(space->index());
  auto* normal_page = new (memory) NormalPage(heap, space);
  space->AddPage(normal_page);
  space->AddToFreeList(normal_page->PayloadStart(), normal_page->PayloadSize());
  return normal_page;
}

// static
void NormalPage::Destroy(NormalPage* page) {
  DCHECK(page);
  BaseSpace* space = page->space();
  DCHECK_EQ(space->end(), std::find(space->begin(), space->end(), page));
  page->~NormalPage();
  PageBackend* backend = page->heap()->page_backend();
  backend->FreeNormalPageMemory(space->index(),
                                reinterpret_cast<Address>(page));
}

NormalPage::NormalPage(Heap* heap, BaseSpace* space)
    : BasePage(heap, space, PageType::kNormal),
      object_start_bitmap_(PayloadStart()) {
  DCHECK_LT(kLargeObjectSizeThreshold,
            static_cast<size_t>(PayloadEnd() - PayloadStart()));
}

NormalPage::~NormalPage() = default;

NormalPage::iterator NormalPage::begin() {
  const auto& lab = NormalPageSpace::From(space())->linear_allocation_buffer();
  return iterator(reinterpret_cast<HeapObjectHeader*>(PayloadStart()),
                  lab.start(), lab.size());
}

NormalPage::const_iterator NormalPage::begin() const {
  const auto& lab = NormalPageSpace::From(space())->linear_allocation_buffer();
  return const_iterator(
      reinterpret_cast<const HeapObjectHeader*>(PayloadStart()), lab.start(),
      lab.size());
}

Address NormalPage::PayloadStart() {
  return AlignAddress((reinterpret_cast<Address>(this + 1)),
                      kAllocationGranularity);
}

ConstAddress NormalPage::PayloadStart() const {
  return const_cast<NormalPage*>(this)->PayloadStart();
}

Address NormalPage::PayloadEnd() { return PayloadStart() + PayloadSize(); }

ConstAddress NormalPage::PayloadEnd() const {
  return const_cast<NormalPage*>(this)->PayloadEnd();
}

// static
size_t NormalPage::PayloadSize() {
  const size_t header_size =
      RoundUp(sizeof(NormalPage), kAllocationGranularity);
  return kPageSize - 2 * kGuardPageSize - header_size;
}

LargePage::LargePage(Heap* heap, BaseSpace* space, size_t size)
    : BasePage(heap, space, PageType::kLarge), payload_size_(size) {}

LargePage::~LargePage() = default;

// static
LargePage* LargePage::Create(LargePageSpace* space, size_t size) {
  DCHECK(space);
  DCHECK_LE(kLargeObjectSizeThreshold, size);
  const size_t page_header_size =
      RoundUp(sizeof(LargePage), kAllocationGranularity);
  const size_t allocation_size = page_header_size + size;

  Heap* heap = space->raw_heap()->heap();
  void* memory = heap->page_backend()->AllocateLargePageMemory(allocation_size);
  LargePage* page = new (memory) LargePage(heap, space, size);
  space->AddPage(page);
  return page;
}

// static
void LargePage::Destroy(LargePage* page) {
  DCHECK(page);
#if DEBUG
  BaseSpace* space = page->space();
  DCHECK_EQ(space->end(), std::find(space->begin(), space->end(), page));
#endif
  page->~LargePage();
  PageBackend* backend = page->heap()->page_backend();
  backend->FreeLargePageMemory(reinterpret_cast<Address>(page));
}

HeapObjectHeader* LargePage::ObjectHeader() {
  return reinterpret_cast<HeapObjectHeader*>(PayloadStart());
}

const HeapObjectHeader* LargePage::ObjectHeader() const {
  return reinterpret_cast<const HeapObjectHeader*>(PayloadStart());
}

Address LargePage::PayloadStart() {
  return AlignAddress((reinterpret_cast<Address>(this + 1)),
                      kAllocationGranularity);
}

ConstAddress LargePage::PayloadStart() const {
  return const_cast<LargePage*>(this)->PayloadStart();
}

Address LargePage::PayloadEnd() { return PayloadStart() + PayloadSize(); }

ConstAddress LargePage::PayloadEnd() const {
  return const_cast<LargePage*>(this)->PayloadEnd();
}

}  // namespace internal
}  // namespace cppgc
