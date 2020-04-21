// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap-page.h"
#include "include/cppgc/internal/api-constants.h"
#include "src/heap/cppgc/globals.h"

namespace cppgc {
namespace internal {

namespace {

Address AlignAddress(Address address, size_t alignment) {
  return reinterpret_cast<Address>(
      RoundUp(reinterpret_cast<uintptr_t>(address), alignment));
}

}  // namespace

STATIC_ASSERT(kPageSize == api_constants::kPageAlignment);

BasePage::BasePage(Heap* heap) : heap_(heap) {
  DCHECK_EQ(0u, reinterpret_cast<uintptr_t>(this) & kPageOffsetMask);
  DCHECK_EQ(reinterpret_cast<void*>(&heap_),
            FromPayload(this) + api_constants::kHeapOffset);
}

// static
BasePage* BasePage::FromPayload(const void* payload) {
  return reinterpret_cast<BasePage*>(
      reinterpret_cast<uintptr_t>(const_cast<void*>(payload)) & kPageBaseMask);
}

// static
NormalPage* NormalPage::Create(Heap* heap) {
  Address reservation = reinterpret_cast<Address>(calloc(1, 2 * kPageSize));
  return new (AlignAddress(reservation, kPageSize))
      NormalPage(heap, reservation);
}

// static
void NormalPage::Destroy(NormalPage* page) {
  Address reservation = page->reservation_;
  page->~NormalPage();
  free(reservation);
}

NormalPage::NormalPage(Heap* heap, Address reservation)
    : BasePage(heap),
      reservation_(reservation),
      payload_start_(AlignAddress(reinterpret_cast<Address>(this + 1),
                                  kAllocationGranularity)),
      payload_end_(reinterpret_cast<Address>(this) + kPageSize) {
  DCHECK_GT(PayloadEnd() - PayloadStart(), kLargeObjectSizeThreshold);
}

}  // namespace internal
}  // namespace cppgc
