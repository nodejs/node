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

void AgeTable::SetAgeForRange(uintptr_t offset_begin, uintptr_t offset_end,
                              Age age,
                              AdjacentCardsPolicy adjacent_cards_policy) {
  // First, mark inner cards.
  const uintptr_t inner_card_offset_begin =
      RoundUp(offset_begin, kCardSizeInBytes);
  const uintptr_t outer_card_offset_end =
      RoundDown(offset_end, kCardSizeInBytes);

  for (auto inner_offset = inner_card_offset_begin;
       inner_offset < outer_card_offset_end; inner_offset += kCardSizeInBytes)
    SetAge(inner_offset, age);

  // If outer cards are not card-aligned and are not of the same age, mark them
  // as mixed.
  const auto set_age_for_outer_card =
      [this, age, adjacent_cards_policy](uintptr_t offset) {
        if (IsAligned(offset, kCardSizeInBytes)) return;
        if (adjacent_cards_policy == AdjacentCardsPolicy::kIgnore)
          SetAge(offset, age);
        else if (GetAge(offset) != age)
          SetAge(offset, AgeTable::Age::kMixed);
      };

  set_age_for_outer_card(offset_begin);
  set_age_for_outer_card(offset_end);
}

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
