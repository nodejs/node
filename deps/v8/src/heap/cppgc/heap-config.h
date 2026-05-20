// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_CONFIG_H_
#define V8_HEAP_CPPGC_HEAP_CONFIG_H_

#include "include/cppgc/heap.h"
#include "src/base/platform/time.h"

namespace cppgc::internal {

using StackState = cppgc::Heap::StackState;

enum class CollectionType : uint8_t {
  kMinor,
  kMajor,
};

// Specifies how free memory should be returned to the system.
enum class FreeMemoryHandling : uint8_t {
  // Do not discard or decommit free memory.
  kRetainMemory,
  // Return memory to the system as much as possible:
  // - Discard free address ranges as they are found, if supported by the
  //   current build configuration (see CanDiscardMemory()).
  // - Decommit pooled pages at the end of a sweep cycle.
  kReleaseMemory
};

struct MarkingConfig {
  using MarkingType = cppgc::Heap::MarkingType;
  enum class IsForcedGC : uint8_t {
    kNotForced,
    kForced,
  };

  static constexpr MarkingConfig Default() { return {}; }

  const CollectionType collection_type = CollectionType::kMajor;
  StackState stack_state = StackState::kMayContainHeapPointers;
  MarkingType marking_type = MarkingType::kIncremental;
  IsForcedGC is_forced_gc = IsForcedGC::kNotForced;
};

struct SweepingConfig {
  using SweepingType = cppgc::Heap::SweepingType;
  enum class CompactableSpaceHandling { kSweep, kIgnore };
  using FreeMemoryHandling = cppgc::internal::FreeMemoryHandling;

  SweepingType sweeping_type = SweepingType::kIncrementalAndConcurrent;
  CompactableSpaceHandling compactable_space_handling =
      CompactableSpaceHandling::kSweep;
  FreeMemoryHandling free_memory_handling = FreeMemoryHandling::kRetainMemory;
};

struct GCConfig {
  using MarkingType = MarkingConfig::MarkingType;
  using SweepingType = SweepingConfig::SweepingType;
  using FreeMemoryHandling = SweepingConfig::FreeMemoryHandling;
  using IsForcedGC = MarkingConfig::IsForcedGC;

  static constexpr GCConfig ConservativeAtomicConfig() {
    return {CollectionType::kMajor, StackState::kMayContainHeapPointers,
            MarkingType::kAtomic, SweepingType::kAtomic};
  }

  static constexpr GCConfig PreciseAtomicConfig() {
    return {CollectionType::kMajor, StackState::kNoHeapPointers,
            MarkingType::kAtomic, SweepingType::kAtomic};
  }

  static constexpr GCConfig ConservativeIncrementalConfig() {
    return {CollectionType::kMajor, StackState::kMayContainHeapPointers,
            MarkingType::kIncremental, SweepingType::kAtomic};
  }

  static constexpr GCConfig PreciseIncrementalConfig() {
    return {CollectionType::kMajor, StackState::kNoHeapPointers,
            MarkingType::kIncremental, SweepingType::kAtomic};
  }

  static constexpr GCConfig
  PreciseIncrementalMarkingConcurrentSweepingConfig() {
    return {CollectionType::kMajor, StackState::kNoHeapPointers,
            MarkingType::kIncremental, SweepingType::kIncrementalAndConcurrent};
  }

  static constexpr GCConfig PreciseConcurrentConfig() {
    return {CollectionType::kMajor, StackState::kNoHeapPointers,
            MarkingType::kIncrementalAndConcurrent,
            SweepingType::kIncrementalAndConcurrent};
  }

  static constexpr GCConfig MinorPreciseAtomicConfig() {
    return {CollectionType::kMinor, StackState::kNoHeapPointers,
            MarkingType::kAtomic, SweepingType::kAtomic};
  }

  static constexpr GCConfig MinorConservativeAtomicConfig() {
    return {CollectionType::kMinor, StackState::kMayContainHeapPointers,
            MarkingType::kAtomic, SweepingType::kAtomic};
  }

  CollectionType collection_type = CollectionType::kMajor;
  StackState stack_state = StackState::kMayContainHeapPointers;
  MarkingType marking_type = MarkingType::kAtomic;
  SweepingType sweeping_type = SweepingType::kAtomic;
  FreeMemoryHandling free_memory_handling = FreeMemoryHandling::kRetainMemory;
  IsForcedGC is_forced_gc = IsForcedGC::kNotForced;
};

}  // namespace cppgc::internal

#endif  // V8_HEAP_CPPGC_HEAP_CONFIG_H_
