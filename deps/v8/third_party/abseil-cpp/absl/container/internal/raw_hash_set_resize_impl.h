// Copyright 2025 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This is a private implementation detail of resize algorithm of
// raw_hash_set. It is exposed in a separate file for testing purposes.

#ifndef ABSL_CONTAINER_INTERNAL_RAW_HASH_SET_RESIZE_IMPL_H_
#define ABSL_CONTAINER_INTERNAL_RAW_HASH_SET_RESIZE_IMPL_H_

#include <cstddef>
#include <cstdint>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {

// Encoding for probed elements used for smaller tables.
// Data is encoded into single integer.
// Storage format for 4 bytes:
// - 7 bits for h2
// - 12 bits for source_offset
// - 13 bits for h1
// Storage format for 8 bytes:
// - 7 bits for h2
// - 28 bits for source_offset
// - 29 bits for h1
// Storage format for 16 bytes:
// - 7 bits for h2
// - 57 bits for source_offset
// - 58 bits for h1
template <typename IntType, size_t kTotalBits>
struct ProbedItemImpl {
  static constexpr IntType kH2Bits = 7;

  static constexpr IntType kMaxOldBits = (kTotalBits - kH2Bits) / 2;
  static constexpr IntType kMaxOldCapacity = (IntType{1} << kMaxOldBits) - 1;

  // We always have one bit more for h1.
  static constexpr IntType kMaxNewBits = kMaxOldBits + 1;
  static constexpr IntType kMaxNewCapacity = (IntType{1} << kMaxNewBits) - 1;

  static_assert(kMaxNewBits + kMaxOldBits + kH2Bits == kTotalBits);

  ProbedItemImpl() = default;
  ProbedItemImpl(uint8_t h2_arg, size_t source_offset_arg, size_t h1_arg)
      : h2(h2_arg),
        source_offset(static_cast<IntType>(source_offset_arg)),
        h1(static_cast<IntType>(h1_arg)) {}

  IntType h2 : kH2Bits;
  IntType source_offset : kMaxOldBits;
  IntType h1 : kMaxNewBits;
};

using ProbedItem4Bytes = ProbedItemImpl<uint32_t, 32>;
static_assert(sizeof(ProbedItem4Bytes) == 4);
using ProbedItem8Bytes = ProbedItemImpl<uint64_t, 64>;
static_assert(sizeof(ProbedItem8Bytes) == 8);
using ProbedItem16Bytes = ProbedItemImpl<uint64_t, 7 + 57 + 58>;
static_assert(sizeof(ProbedItem16Bytes) == 16);

}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_CONTAINER_INTERNAL_RAW_HASH_SET_RESIZE_IMPL_H_
