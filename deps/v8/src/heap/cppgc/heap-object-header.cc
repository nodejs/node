// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap-object-header.h"

#include "include/cppgc/internal/api-constants.h"
#include "src/base/macros.h"
#include "src/base/sanitizer/asan.h"
#include "src/heap/cppgc/gc-info-table.h"
#include "src/heap/cppgc/heap-page.h"

namespace cppgc {
namespace internal {

STATIC_ASSERT((kAllocationGranularity % sizeof(HeapObjectHeader)) == 0);

void HeapObjectHeader::CheckApiConstants() {
  STATIC_ASSERT(api_constants::kFullyConstructedBitMask ==
                FullyConstructedField::kMask);
  STATIC_ASSERT(api_constants::kFullyConstructedBitFieldOffsetFromPayload ==
                (sizeof(encoded_high_) + sizeof(encoded_low_)));
}

void HeapObjectHeader::Finalize() {
#ifdef V8_USE_ADDRESS_SANITIZER
  const size_t size =
      IsLargeObject()
          ? LargePage::From(BasePage::FromPayload(this))->ObjectSize()
          : ObjectSize();
  ASAN_UNPOISON_MEMORY_REGION(ObjectStart(), size);
#endif  // V8_USE_ADDRESS_SANITIZER
  const GCInfo& gc_info = GlobalGCInfoTable::GCInfoFromIndex(GetGCInfoIndex());
  if (gc_info.finalize) {
    gc_info.finalize(ObjectStart());
  }
}

HeapObjectName HeapObjectHeader::GetName() const {
  const GCInfo& gc_info = GlobalGCInfoTable::GCInfoFromIndex(GetGCInfoIndex());
  return gc_info.name(ObjectStart());
}

}  // namespace internal
}  // namespace cppgc
