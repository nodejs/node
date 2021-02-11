// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap-page.h"

#include <algorithm>

#include "include/cppgc/internal/api-constants.h"
#include "src/base/logging.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-space.h"
#include "src/heap/cppgc/heap.h"
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

// static
BasePage* BasePage::FromInnerAddress(const HeapBase* heap, void* address) {
  return const_cast<BasePage*>(
      FromInnerAddress(heap, const_cast<const void*>(address)));
}

// static
const BasePage* BasePage::FromInnerAddress(const HeapBase* heap,
                                           const void* address) {
  return reinterpret_cast<const BasePage*>(
      heap->page_backend()->Lookup(static_cast<ConstAddress>(address)));
}

// static
void BasePage::Destroy(BasePage* page) {
  if (page->is_large()) {
    LargePage::Destroy(LargePage::From(page));
  } else {
    NormalPage::Destroy(NormalPage::From(page));
  }
}

Address BasePage::PayloadStart() {
  return is_large() ? LargePage::From(this)->PayloadStart()
                    : NormalPage::From(this)->PayloadStart();
}

ConstAddress BasePage::PayloadStart() const {
  return const_cast<BasePage*>(this)->PayloadStart();
}

Address BasePage::PayloadEnd() {
  return is_large() ? LargePage::From(this)->PayloadEnd()
                    : NormalPage::From(this)->PayloadEnd();
}

ConstAddress BasePage::PayloadEnd() const {
  return const_cast<BasePage*>(this)->PayloadEnd();
}

HeapObjectHeader* BasePage::TryObjectHeaderFromInnerAddress(
    void* address) const {
  return const_cast<HeapObjectHeader*>(
      TryObjectHeaderFromInnerAddress(const_cast<const void*>(address)));
}

const HeapObjectHeader* BasePage::TryObjectHeaderFromInnerAddress(
    const void* address) const {
  if (is_large()) {
    if (!LargePage::From(this)->PayloadContains(
            static_cast<ConstAddress>(address)))
      return nullptr;
  } else {
    const NormalPage* normal_page = NormalPage::From(this);
    if (!normal_page->PayloadContains(static_cast<ConstAddress>(address)))
      return nullptr;
    // Check that the space has no linear allocation buffer.
    DCHECK(!NormalPageSpace::From(normal_page->space())
                ->linear_allocation_buffer()
                .size());
  }

  // |address| is on the heap, so we FromInnerAddress can get the header.
  const HeapObjectHeader* header =
      ObjectHeaderFromInnerAddressImpl(this, address);
  if (header->IsFree()) return nullptr;
  DCHECK_NE(kFreeListGCInfoIndex, header->GetGCInfoIndex());
  return header;
}

BasePage::BasePage(HeapBase* heap, BaseSpace* space, PageType type)
    : heap_(heap), space_(space), type_(type) {
  DCHECK_EQ(0u, (reinterpret_cast<uintptr_t>(this) - kGuardPageSize) &
                    kPageOffsetMask);
  DCHECK_EQ(&heap_->raw_heap(), space_->raw_heap());
}

// static
NormalPage* NormalPage::Create(PageBackend* page_backend,
                               NormalPageSpace* space) {
  DCHECK_NOT_NULL(page_backend);
  DCHECK_NOT_NULL(space);
  void* memory = page_backend->AllocateNormalPageMemory(space->index());
  auto* normal_page = new (memory) NormalPage(space->raw_heap()->heap(), space);
  normal_page->SynchronizedStore();
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

NormalPage::NormalPage(HeapBase* heap, BaseSpace* space)
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

LargePage::LargePage(HeapBase* heap, BaseSpace* space, size_t size)
    : BasePage(heap, space, PageType::kLarge), payload_size_(size) {}

LargePage::~LargePage() = default;

// static
LargePage* LargePage::Create(PageBackend* page_backend, LargePageSpace* space,
                             size_t size) {
  DCHECK_NOT_NULL(page_backend);
  DCHECK_NOT_NULL(space);
  DCHECK_LE(kLargeObjectSizeThreshold, size);

  const size_t page_header_size =
      RoundUp(sizeof(LargePage), kAllocationGranularity);
  const size_t allocation_size = page_header_size + size;

  auto* heap = space->raw_heap()->heap();
  void* memory = page_backend->AllocateLargePageMemory(allocation_size);
  LargePage* page = new (memory) LargePage(heap, space, size);
  page->SynchronizedStore();
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
