// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/hash/internal/hash.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/unaligned_access.h"
#include "absl/base/optimization.h"
#include "absl/base/prefetch.h"
#include "absl/hash/internal/city.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace hash_internal {

namespace {

uint64_t Mix32Bytes(const uint8_t* ptr, uint64_t current_state) {
  uint64_t a = absl::base_internal::UnalignedLoad64(ptr);
  uint64_t b = absl::base_internal::UnalignedLoad64(ptr + 8);
  uint64_t c = absl::base_internal::UnalignedLoad64(ptr + 16);
  uint64_t d = absl::base_internal::UnalignedLoad64(ptr + 24);

  uint64_t cs0 = Mix(a ^ kStaticRandomData[1], b ^ current_state);
  uint64_t cs1 = Mix(c ^ kStaticRandomData[2], d ^ current_state);
  return cs0 ^ cs1;
}

[[maybe_unused]] uint64_t LowLevelHashLenGt32(const void* data, size_t len,
                                              uint64_t seed) {
  assert(len > 32);
  const uint8_t* ptr = static_cast<const uint8_t*>(data);
  uint64_t current_state = seed ^ kStaticRandomData[0] ^ len;
  const uint8_t* last_32_ptr = ptr + len - 32;

  if (len > 64) {
    // If we have more than 64 bytes, we're going to handle chunks of 64
    // bytes at a time. We're going to build up four separate hash states
    // which we will then hash together. This avoids short dependency chains.
    uint64_t duplicated_state0 = current_state;
    uint64_t duplicated_state1 = current_state;
    uint64_t duplicated_state2 = current_state;

    do {
      PrefetchToLocalCache(ptr + 5 * ABSL_CACHELINE_SIZE);

      uint64_t a = absl::base_internal::UnalignedLoad64(ptr);
      uint64_t b = absl::base_internal::UnalignedLoad64(ptr + 8);
      uint64_t c = absl::base_internal::UnalignedLoad64(ptr + 16);
      uint64_t d = absl::base_internal::UnalignedLoad64(ptr + 24);
      uint64_t e = absl::base_internal::UnalignedLoad64(ptr + 32);
      uint64_t f = absl::base_internal::UnalignedLoad64(ptr + 40);
      uint64_t g = absl::base_internal::UnalignedLoad64(ptr + 48);
      uint64_t h = absl::base_internal::UnalignedLoad64(ptr + 56);

      current_state = Mix(a ^ kStaticRandomData[1], b ^ current_state);
      duplicated_state0 = Mix(c ^ kStaticRandomData[2], d ^ duplicated_state0);

      duplicated_state1 = Mix(e ^ kStaticRandomData[3], f ^ duplicated_state1);
      duplicated_state2 = Mix(g ^ kStaticRandomData[4], h ^ duplicated_state2);

      ptr += 64;
      len -= 64;
    } while (len > 64);

    current_state = (current_state ^ duplicated_state0) ^
                    (duplicated_state1 + duplicated_state2);
  }

  // We now have a data `ptr` with at most 64 bytes and the current state
  // of the hashing state machine stored in current_state.
  if (len > 32) {
    current_state = Mix32Bytes(ptr, current_state);
  }

  // We now have a data `ptr` with at most 32 bytes and the current state
  // of the hashing state machine stored in current_state. But we can
  // safely read from `ptr + len - 32`.
  return Mix32Bytes(last_32_ptr, current_state);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE inline uint64_t HashBlockOn32Bit(
    const unsigned char* data, size_t len, uint64_t state) {
  // TODO(b/417141985): expose and use CityHash32WithSeed.
  // Note: we can't use PrecombineLengthMix here because len can be up to 1024.
  return CombineRawImpl(
      state + len,
      hash_internal::CityHash32(reinterpret_cast<const char*>(data), len));
}

ABSL_ATTRIBUTE_NOINLINE uint64_t
SplitAndCombineOn32Bit(const unsigned char* first, size_t len, uint64_t state) {
  while (len >= PiecewiseChunkSize()) {
    state = HashBlockOn32Bit(first, PiecewiseChunkSize(), state);
    len -= PiecewiseChunkSize();
    first += PiecewiseChunkSize();
  }
  // Do not call CombineContiguousImpl for empty range since it is modifying
  // state.
  if (len == 0) {
    return state;
  }
  // Handle the remainder.
  return CombineContiguousImpl(state, first, len,
                               std::integral_constant<int, 4>{});
}

ABSL_ATTRIBUTE_ALWAYS_INLINE inline uint64_t HashBlockOn64Bit(
    const unsigned char* data, size_t len, uint64_t state) {
#ifdef ABSL_HAVE_INTRINSIC_INT128
  return LowLevelHashLenGt32(data, len, state);
#else
  return hash_internal::CityHash64WithSeed(reinterpret_cast<const char*>(data),
                                           len, state);
#endif
}

ABSL_ATTRIBUTE_NOINLINE uint64_t
SplitAndCombineOn64Bit(const unsigned char* first, size_t len, uint64_t state) {
  while (len >= PiecewiseChunkSize()) {
    state = HashBlockOn64Bit(first, PiecewiseChunkSize(), state);
    len -= PiecewiseChunkSize();
    first += PiecewiseChunkSize();
  }
  // Do not call CombineContiguousImpl for empty range since it is modifying
  // state.
  if (len == 0) {
    return state;
  }
  // Handle the remainder.
  return CombineContiguousImpl(state, first, len,
                               std::integral_constant<int, 8>{});
}

}  // namespace

uint64_t CombineLargeContiguousImplOn32BitLengthGt8(const unsigned char* first,
                                                    size_t len,
                                                    uint64_t state) {
  assert(len > 8);
  assert(sizeof(size_t) == 4);  // NOLINT(misc-static-assert)
  if (ABSL_PREDICT_TRUE(len <= PiecewiseChunkSize())) {
    return HashBlockOn32Bit(first, len, state);
  }
  return SplitAndCombineOn32Bit(first, len, state);
}

uint64_t CombineLargeContiguousImplOn64BitLengthGt32(const unsigned char* first,
                                                     size_t len,
                                                     uint64_t state) {
  assert(len > 32);
  assert(sizeof(size_t) == 8);  // NOLINT(misc-static-assert)
  if (ABSL_PREDICT_TRUE(len <= PiecewiseChunkSize())) {
    return HashBlockOn64Bit(first, len, state);
  }
  return SplitAndCombineOn64Bit(first, len, state);
}

ABSL_CONST_INIT const void* const MixingHashState::kSeed = &kSeed;

}  // namespace hash_internal
ABSL_NAMESPACE_END
}  // namespace absl
