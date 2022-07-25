// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_CAGED_HEAP_LOCAL_DATA_H_
#define INCLUDE_CPPGC_INTERNAL_CAGED_HEAP_LOCAL_DATA_H_

#include <array>
#include <cstddef>
#include <cstdint>

#include "cppgc/internal/api-constants.h"
#include "cppgc/internal/logging.h"
#include "cppgc/platform.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {
namespace internal {

class HeapBase;

#if defined(CPPGC_YOUNG_GENERATION)

// AgeTable is the bytemap needed for the fast generation check in the write
// barrier. AgeTable contains entries that correspond to 512 bytes memory
// regions (cards). Each entry in the table represents generation of the objects
// that reside on the corresponding card (young, old or mixed).
class AgeTable final {
  static constexpr size_t kRequiredSize = 1 * api_constants::kMB;
  static constexpr size_t kAllocationGranularity =
      api_constants::kAllocationGranularity;

 public:
  enum class Age : uint8_t { kOld, kYoung, kMixed };

  static constexpr size_t kCardSizeInBytes =
      (api_constants::kCagedHeapReservationSize / kAllocationGranularity) /
      kRequiredSize;

  void SetAge(uintptr_t cage_offset, Age age) {
    table_[card(cage_offset)] = age;
  }
  V8_INLINE Age GetAge(uintptr_t cage_offset) const {
    return table_[card(cage_offset)];
  }

  void Reset(PageAllocator* allocator);

 private:
  V8_INLINE size_t card(uintptr_t offset) const {
    constexpr size_t kGranularityBits =
        __builtin_ctz(static_cast<uint32_t>(kCardSizeInBytes));
    const size_t entry = offset >> kGranularityBits;
    CPPGC_DCHECK(table_.size() > entry);
    return entry;
  }

  std::array<Age, kRequiredSize> table_;
};

static_assert(sizeof(AgeTable) == 1 * api_constants::kMB,
              "Size of AgeTable is 1MB");

#endif  // CPPGC_YOUNG_GENERATION

struct CagedHeapLocalData final {
  CagedHeapLocalData(HeapBase&, PageAllocator&);

  bool is_incremental_marking_in_progress = false;
  HeapBase& heap_base;
#if defined(CPPGC_YOUNG_GENERATION)
  AgeTable age_table;
#endif
};

}  // namespace internal
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_INTERNAL_CAGED_HEAP_LOCAL_DATA_H_
