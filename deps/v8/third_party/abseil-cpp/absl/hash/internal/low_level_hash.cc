// Copyright 2020 The Abseil Authors
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

#include "absl/hash/internal/low_level_hash.h"

#include "absl/base/internal/unaligned_access.h"
#include "absl/base/prefetch.h"
#include "absl/numeric/int128.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace hash_internal {

static uint64_t Mix(uint64_t v0, uint64_t v1) {
  absl::uint128 p = v0;
  p *= v1;
  return absl::Uint128Low64(p) ^ absl::Uint128High64(p);
}

uint64_t LowLevelHash(const void* data, size_t len, uint64_t seed,
                      const uint64_t salt[5]) {
  // Prefetch the cacheline that data resides in.
  PrefetchToLocalCache(data);
  const uint8_t* ptr = static_cast<const uint8_t*>(data);
  uint64_t starting_length = static_cast<uint64_t>(len);
  uint64_t current_state = seed ^ salt[0];

  if (len > 64) {
    // If we have more than 64 bytes, we're going to handle chunks of 64
    // bytes at a time. We're going to build up two separate hash states
    // which we will then hash together.
    uint64_t duplicated_state = current_state;

    do {
      // Always prefetch the next cacheline.
      PrefetchToLocalCache(ptr + ABSL_CACHELINE_SIZE);

      uint64_t a = absl::base_internal::UnalignedLoad64(ptr);
      uint64_t b = absl::base_internal::UnalignedLoad64(ptr + 8);
      uint64_t c = absl::base_internal::UnalignedLoad64(ptr + 16);
      uint64_t d = absl::base_internal::UnalignedLoad64(ptr + 24);
      uint64_t e = absl::base_internal::UnalignedLoad64(ptr + 32);
      uint64_t f = absl::base_internal::UnalignedLoad64(ptr + 40);
      uint64_t g = absl::base_internal::UnalignedLoad64(ptr + 48);
      uint64_t h = absl::base_internal::UnalignedLoad64(ptr + 56);

      uint64_t cs0 = Mix(a ^ salt[1], b ^ current_state);
      uint64_t cs1 = Mix(c ^ salt[2], d ^ current_state);
      current_state = (cs0 ^ cs1);

      uint64_t ds0 = Mix(e ^ salt[3], f ^ duplicated_state);
      uint64_t ds1 = Mix(g ^ salt[4], h ^ duplicated_state);
      duplicated_state = (ds0 ^ ds1);

      ptr += 64;
      len -= 64;
    } while (len > 64);

    current_state = current_state ^ duplicated_state;
  }

  // We now have a data `ptr` with at most 64 bytes and the current state
  // of the hashing state machine stored in current_state.
  while (len > 16) {
    uint64_t a = absl::base_internal::UnalignedLoad64(ptr);
    uint64_t b = absl::base_internal::UnalignedLoad64(ptr + 8);

    current_state = Mix(a ^ salt[1], b ^ current_state);

    ptr += 16;
    len -= 16;
  }

  // We now have a data `ptr` with at most 16 bytes.
  uint64_t a = 0;
  uint64_t b = 0;
  if (len > 8) {
    // When we have at least 9 and at most 16 bytes, set A to the first 64
    // bits of the input and B to the last 64 bits of the input. Yes, they will
    // overlap in the middle if we are working with less than the full 16
    // bytes.
    a = absl::base_internal::UnalignedLoad64(ptr);
    b = absl::base_internal::UnalignedLoad64(ptr + len - 8);
  } else if (len > 3) {
    // If we have at least 4 and at most 8 bytes, set A to the first 32
    // bits and B to the last 32 bits.
    a = absl::base_internal::UnalignedLoad32(ptr);
    b = absl::base_internal::UnalignedLoad32(ptr + len - 4);
  } else if (len > 0) {
    // If we have at least 1 and at most 3 bytes, read all of the provided
    // bits into A, with some adjustments.
    a = static_cast<uint64_t>((ptr[0] << 16) | (ptr[len >> 1] << 8) |
                              ptr[len - 1]);
    b = 0;
  } else {
    a = 0;
    b = 0;
  }

  uint64_t w = Mix(a ^ salt[1], b ^ current_state);
  uint64_t z = salt[1] ^ starting_length;
  return Mix(w, z);
}

}  // namespace hash_internal
ABSL_NAMESPACE_END
}  // namespace absl
