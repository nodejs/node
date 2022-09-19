// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/caged-heap-local-data.h"

#include <algorithm>
#include <type_traits>

#include "include/cppgc/platform.h"
#include "src/base/macros.h"

namespace cppgc {
namespace internal {

CagedHeapLocalData::CagedHeapLocalData(HeapBase& heap_base,
                                       PageAllocator& allocator)
    : heap_base(heap_base) {
#if defined(CPPGC_YOUNG_GENERATION)
  age_table.Reset(&allocator);
#endif  // defined(CPPGC_YOUNG_GENERATION)
}

#if defined(CPPGC_YOUNG_GENERATION)

static_assert(
    std::is_trivially_default_constructible<AgeTable>::value,
    "To support lazy committing, AgeTable must be trivially constructible");

void AgeTable::Reset(PageAllocator* allocator) {
  // TODO(chromium:1029379): Consider MADV_DONTNEED instead of MADV_FREE on
  // POSIX platforms.
  std::fill(table_.begin(), table_.end(), Age::kOld);
  const uintptr_t begin = RoundUp(reinterpret_cast<uintptr_t>(table_.data()),
                                  allocator->CommitPageSize());
  const uintptr_t end =
      RoundDown(reinterpret_cast<uintptr_t>(table_.data() + table_.size()),
                allocator->CommitPageSize());

  allocator->DiscardSystemPages(reinterpret_cast<void*>(begin), end - begin);
}

#endif  // defined(CPPGC_YOUNG_GENERATION)

}  // namespace internal
}  // namespace cppgc
