// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_PRECISE_ZONE_STATS

#if defined(__clang__) || defined(__GLIBCXX__)
#include <cxxabi.h>
#endif  // __GLIBCXX__
#include <cinttypes>
#include <cstdio>

#include "src/base/platform/wrappers.h"
#include "src/utils/utils.h"
#include "src/zone/type-stats.h"

namespace v8 {
namespace internal {

void TypeStats::MergeWith(const TypeStats& other) {
  for (auto const& item : other.map_) {
    Add(item.first, item.second);
  }
}

class Demangler {
 public:
  Demangler() = default;
  ~Demangler() {
    if (buffer_) base::Free(buffer_);
    USE(buffer_len_);  // In case demangling is not supported.
  }

  const char* demangle(std::type_index type_id) {
#if defined(__clang__) || defined(__GLIBCXX__)
    int status = -1;
    char* result =
        abi::__cxa_demangle(type_id.name(), buffer_, &buffer_len_, &status);
    if (status == 0) {
      // Upon success, the buffer_ may be reallocated.
      buffer_ = result;
      return buffer_;
    }
#endif
    return type_id.name();
  }

 private:
  char* buffer_ = nullptr;
  size_t buffer_len_ = 0;
};

void TypeStats::Dump() const {
  Demangler d;
  PrintF("===== TypeStats =====\n");
  PrintF("-------------+--------------+------------+--------+--------------\n");
  PrintF("       alloc |      dealloc |      count | sizeof | name\n");
  PrintF("-------------+--------------+------------+--------+--------------\n");
  uint64_t total_allocation_count = 0;
  uint64_t total_allocated_bytes = 0;
  uint64_t total_deallocated_bytes = 0;
  for (auto const& item : map_) {
    const StatsEntry& entry = item.second;
    total_allocation_count += entry.allocation_count;
    total_allocated_bytes += entry.allocated_bytes;
    total_deallocated_bytes += entry.deallocated_bytes;
    PrintF("%12zu | %12zu | %10zu | %6zu | %s\n", entry.allocated_bytes,
           entry.deallocated_bytes, entry.allocation_count, entry.instance_size,
           d.demangle(item.first));
  }
  PrintF("%12" PRIu64 " | %12" PRIu64 " | %10" PRIu64
         " | ===== TOTAL STATS =====\n",
         total_allocated_bytes, total_deallocated_bytes,
         total_allocation_count);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_PRECISE_ZONE_STATS
