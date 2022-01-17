// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_CAGED_HEAP_LOCAL_DATA_H_
#define INCLUDE_CPPGC_INTERNAL_CAGED_HEAP_LOCAL_DATA_H_

#include <array>

#include "cppgc/internal/api-constants.h"
#include "cppgc/internal/logging.h"
#include "cppgc/platform.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {
namespace internal {

class HeapBase;

#if defined(CPPGC_YOUNG_GENERATION)

// AgeTable contains entries that correspond to 4KB memory regions. Each entry
// can be in one of three states: kOld, kYoung or kUnknown.
class AgeTable final {
  static constexpr size_t kGranularityBits = 12;  // 4KiB per byte.

 public:
  enum class Age : uint8_t { kOld, kYoung, kUnknown };

  static constexpr size_t kEntrySizeInBytes = 1 << kGranularityBits;

  Age& operator[](uintptr_t offset) { return table_[entry(offset)]; }
  Age operator[](uintptr_t offset) const { return table_[entry(offset)]; }

  void Reset(PageAllocator* allocator);

 private:
  static constexpr size_t kAgeTableSize =
      api_constants::kCagedHeapReservationSize >> kGranularityBits;

  size_t entry(uintptr_t offset) const {
    const size_t entry = offset >> kGranularityBits;
    CPPGC_DCHECK(table_.size() > entry);
    return entry;
  }

  std::array<Age, kAgeTableSize> table_;
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
