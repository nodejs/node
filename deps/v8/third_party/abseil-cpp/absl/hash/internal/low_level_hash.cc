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

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "absl/base/config.h"
#include "absl/base/internal/unaligned_access.h"
#include "absl/base/optimization.h"
#include "absl/base/prefetch.h"
#include "absl/numeric/int128.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace hash_internal {
namespace {
uint64_t Mix(uint64_t v0, uint64_t v1) {
  absl::uint128 p = v0;
  p *= v1;
  return absl::Uint128Low64(p) ^ absl::Uint128High64(p);
}
uint64_t Mix32Bytes(const uint8_t* ptr, uint64_t current_state,
                    const uint64_t salt[5]) {
  uint64_t a = absl::base_internal::UnalignedLoad64(ptr);
  uint64_t b = absl::base_internal::UnalignedLoad64(ptr + 8);
  uint64_t c = absl::base_internal::UnalignedLoad64(ptr + 16);
  uint64_t d = absl::base_internal::UnalignedLoad64(ptr + 24);

  uint64_t cs0 = Mix(a ^ salt[1], b ^ current_state);
  uint64_t cs1 = Mix(c ^ salt[2], d ^ current_state);
  return cs0 ^ cs1;
}
}  // namespace

uint64_t LowLevelHashLenGt32(const void* data, size_t len, uint64_t seed,
                             const uint64_t salt[5]) {
  assert(len > 32);
  const uint8_t* ptr = static_cast<const uint8_t*>(data);
  uint64_t current_state = seed ^ salt[0] ^ len;
  const uint8_t* last_32_ptr = ptr + len - 32;

  if (len > 64) {
    // If we have more than 64 bytes, we're going to handle chunks of 64
    // bytes at a time. We're going to build up four separate hash states
    // which we will then hash together. This avoids short dependency chains.
    uint64_t duplicated_state0 = current_state;
    uint64_t duplicated_state1 = current_state;
    uint64_t duplicated_state2 = current_state;

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

      current_state = Mix(a ^ salt[1], b ^ current_state);
      duplicated_state0 = Mix(c ^ salt[2], d ^ duplicated_state0);

      duplicated_state1 = Mix(e ^ salt[3], f ^ duplicated_state1);
      duplicated_state2 = Mix(g ^ salt[4], h ^ duplicated_state2);

      ptr += 64;
      len -= 64;
    } while (len > 64);

    current_state = (current_state ^ duplicated_state0) ^
                    (duplicated_state1 + duplicated_state2);
  }

  // We now have a data `ptr` with at most 64 bytes and the current state
  // of the hashing state machine stored in current_state.
  if (len > 32) {
    current_state = Mix32Bytes(ptr, current_state, salt);
  }

  // We now have a data `ptr` with at most 32 bytes and the current state
  // of the hashing state machine stored in current_state. But we can
  // safely read from `ptr + len - 32`.
  return Mix32Bytes(last_32_ptr, current_state, salt);
}

}  // namespace hash_internal
ABSL_NAMESPACE_END
}  // namespace absl
