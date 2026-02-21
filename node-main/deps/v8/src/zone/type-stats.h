// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_TYPE_STATS_H_
#define V8_ZONE_TYPE_STATS_H_

#include <iosfwd>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

#include "src/common/globals.h"

namespace v8 {
namespace internal {

class TypeStats;

#ifdef V8_ENABLE_PRECISE_ZONE_STATS

class TypeStats {
 public:
  TypeStats() = default;

  template <typename TypeTag>
  void AddAllocated(size_t bytes) {
    StatsEntry& entry = map_[std::type_index(typeid(TypeTag))];
    entry.allocation_count++;
    entry.allocated_bytes += bytes;
    // sizeof(IncompleteType) is not allowed so record size as a sizeof(char).
    constexpr bool kIsIncomplete =
        std::is_same_v<TypeTag, void> || std::is_array_v<TypeTag>;
    using TypeTagForSizeof =
        typename std::conditional_t<kIsIncomplete, char, TypeTag>;
    entry.instance_size = sizeof(TypeTagForSizeof);
  }

  template <typename TypeTag>
  void AddDeallocated(size_t bytes) {
    StatsEntry& entry = map_[std::type_index(typeid(TypeTag))];
    entry.deallocated_bytes += bytes;
  }

  // Merges other stats into this stats object.
  void MergeWith(const TypeStats& other);

  // Prints recorded statisticts to stdout.
  void Dump() const;

 private:
  struct StatsEntry {
    size_t allocation_count = 0;
    size_t allocated_bytes = 0;
    size_t deallocated_bytes = 0;
    size_t instance_size = 0;
  };

  void Add(std::type_index type_id, const StatsEntry& other_entry) {
    StatsEntry& entry = map_[type_id];
    entry.allocation_count += other_entry.allocation_count;
    entry.allocated_bytes += other_entry.allocated_bytes;
    entry.deallocated_bytes += other_entry.deallocated_bytes;
    entry.instance_size = other_entry.instance_size;
  }

  using HashMap = std::unordered_map<std::type_index, StatsEntry>;
  HashMap map_;
};

#endif  // V8_ENABLE_PRECISE_ZONE_STATS

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_TYPE_STATS_H_
