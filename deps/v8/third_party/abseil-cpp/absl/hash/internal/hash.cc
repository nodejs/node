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

#ifdef ABSL_AES_INTERNAL_HAVE_X86_SIMD
#error ABSL_AES_INTERNAL_HAVE_X86_SIMD cannot be directly set
#elif defined(__SSE4_2__) && defined(__AES__)
#define ABSL_AES_INTERNAL_HAVE_X86_SIMD
#endif


#ifdef ABSL_AES_INTERNAL_HAVE_X86_SIMD
#include <smmintrin.h>
#include <wmmintrin.h>
#include <xmmintrin.h>
#endif  // ABSL_AES_INTERNAL_HAVE_X86_SIMD

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace hash_internal {

namespace {

void PrefetchFutureDataToLocalCache(const uint8_t* ptr) {
  PrefetchToLocalCache(ptr + 5 * ABSL_CACHELINE_SIZE);
}

#ifdef ABSL_AES_INTERNAL_HAVE_X86_SIMD
uint64_t Mix4x16Vectors(__m128i a, __m128i b, __m128i c, __m128i d) {
  // res128 = encrypt(a + c, d) + decrypt(b - d, a)
  auto res128 = _mm_add_epi64(_mm_aesenc_si128(_mm_add_epi64(a, c), d),
                              _mm_aesdec_si128(_mm_sub_epi64(b, d), a));
  auto x64 = static_cast<uint64_t>(_mm_cvtsi128_si64(res128));
  auto y64 = static_cast<uint64_t>(_mm_extract_epi64(res128, 1));
  return x64 ^ y64;
}

uint64_t LowLevelHash33To64(uint64_t seed, const uint8_t* ptr, size_t len) {
  assert(len > 32);
  assert(len <= 64);
  __m128i state =
      _mm_set_epi64x(static_cast<int64_t>(seed), static_cast<int64_t>(len));
  auto a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
  auto b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 16));
  auto* last32_ptr = ptr + len - 32;
  auto c = _mm_loadu_si128(reinterpret_cast<const __m128i*>(last32_ptr));
  auto d = _mm_loadu_si128(reinterpret_cast<const __m128i*>(last32_ptr + 16));

  // Bits of the second argument to _mm_aesdec_si128/_mm_aesenc_si128 are
  // XORed with the state argument after encryption.
  // We use each value as the first argument to shuffle all the bits around.
  // We do not add any salt to the state or loaded data, instead we vary
  // instructions used to mix bits _mm_aesdec_si128/_mm_aesenc_si128 and
  // _mm_add_epi64/_mm_sub_epi64.
  // _mm_add_epi64/_mm_sub_epi64 are combined to one instruction with data
  // loading like `vpaddq  xmm1, xmm0, xmmword ptr [rdi]`.
  auto na = _mm_aesdec_si128(_mm_add_epi64(state, a), state);
  auto nb = _mm_aesdec_si128(_mm_sub_epi64(state, b), state);
  auto nc = _mm_aesenc_si128(_mm_add_epi64(state, c), state);
  auto nd = _mm_aesenc_si128(_mm_sub_epi64(state, d), state);

  // We perform another round of encryption to mix bits between two halves of
  // the input.
  return Mix4x16Vectors(na, nb, nc, nd);
}

[[maybe_unused]] ABSL_ATTRIBUTE_NOINLINE uint64_t
LowLevelHashLenGt64(uint64_t seed, const void* data, size_t len) {
  assert(len > 64);
  const uint8_t* ptr = static_cast<const uint8_t*>(data);
  const uint8_t* last_32_ptr = ptr + len - 32;

  // If we have more than 64 bytes, we're going to handle chunks of 64
  // bytes at a time. We're going to build up four separate hash states
  // which we will then hash together. This avoids short dependency chains.
  __m128i state0 =
      _mm_set_epi64x(static_cast<int64_t>(seed), static_cast<int64_t>(len));
  __m128i state1 = state0;
  __m128i state2 = state1;
  __m128i state3 = state2;

  // Mixing two 128-bit vectors at a time with corresponding states.
  // All variables are mixed slightly differently to avoid hash collision
  // due to trivial byte rotation.
  // We combine state and data with _mm_add_epi64/_mm_sub_epi64 before applying
  // AES encryption to make hash function dependent on the order of the blocks.
  // See comments in LowLevelHash33To64 for more considerations.
  auto mix_ab = [&state0,
                 &state1](const uint8_t* p) ABSL_ATTRIBUTE_ALWAYS_INLINE {
    // i128 a = *p;
    // i128 b = *(p + 16);
    // state0 = decrypt(state0 + a, state0);
    // state1 = decrypt(state1 - b, state1);
    auto a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
    auto b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + 16));
    state0 = _mm_aesdec_si128(_mm_add_epi64(state0, a), state0);
    state1 = _mm_aesdec_si128(_mm_sub_epi64(state1, b), state1);
  };
  auto mix_cd = [&state2,
                 &state3](const uint8_t* p) ABSL_ATTRIBUTE_ALWAYS_INLINE {
    // i128 c = *p;
    // i128 d = *(p + 16);
    // state2 = encrypt(state2 + c, state2);
    // state3 = encrypt(state3 - d, state3);
    auto c = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
    auto d = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + 16));
    state2 = _mm_aesenc_si128(_mm_add_epi64(state2, c), state2);
    state3 = _mm_aesenc_si128(_mm_sub_epi64(state3, d), state3);
  };

  do {
    PrefetchFutureDataToLocalCache(ptr);
    mix_ab(ptr);
    mix_cd(ptr + 32);

    ptr += 64;
    len -= 64;
  } while (len > 64);

  // We now have a data `ptr` with at most 64 bytes.
  if (len > 32) {
    mix_ab(ptr);
  }
  mix_cd(last_32_ptr);

  return Mix4x16Vectors(state0, state1, state2, state3);
}
#else
uint64_t Mix32Bytes(const uint8_t* ptr, uint64_t current_state) {
  uint64_t a = absl::base_internal::UnalignedLoad64(ptr);
  uint64_t b = absl::base_internal::UnalignedLoad64(ptr + 8);
  uint64_t c = absl::base_internal::UnalignedLoad64(ptr + 16);
  uint64_t d = absl::base_internal::UnalignedLoad64(ptr + 24);

  uint64_t cs0 = Mix(a ^ kStaticRandomData[1], b ^ current_state);
  uint64_t cs1 = Mix(c ^ kStaticRandomData[2], d ^ current_state);
  return cs0 ^ cs1;
}

uint64_t LowLevelHash33To64(uint64_t seed, const uint8_t* ptr, size_t len) {
  assert(len > 32);
  assert(len <= 64);
  uint64_t current_state = seed ^ kStaticRandomData[0] ^ len;
  const uint8_t* last_32_ptr = ptr + len - 32;
  return Mix32Bytes(last_32_ptr, Mix32Bytes(ptr, current_state));
}

[[maybe_unused]] ABSL_ATTRIBUTE_NOINLINE uint64_t
LowLevelHashLenGt64(uint64_t seed, const void* data, size_t len) {
  assert(len > 64);
  const uint8_t* ptr = static_cast<const uint8_t*>(data);
  uint64_t current_state = seed ^ kStaticRandomData[0] ^ len;
  const uint8_t* last_32_ptr = ptr + len - 32;
  // If we have more than 64 bytes, we're going to handle chunks of 64
  // bytes at a time. We're going to build up four separate hash states
  // which we will then hash together. This avoids short dependency chains.
  uint64_t duplicated_state0 = current_state;
  uint64_t duplicated_state1 = current_state;
  uint64_t duplicated_state2 = current_state;

  do {
    PrefetchFutureDataToLocalCache(ptr);

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
#endif  // ABSL_AES_INTERNAL_HAVE_X86_SIMD

[[maybe_unused]] uint64_t LowLevelHashLenGt32(uint64_t seed, const void* data,
                                              size_t len) {
  assert(len > 32);
  if (ABSL_PREDICT_FALSE(len > 64)) {
    return LowLevelHashLenGt64(seed, data, len);
  }
  return LowLevelHash33To64(seed, static_cast<const uint8_t*>(data), len);
}

ABSL_ATTRIBUTE_ALWAYS_INLINE inline uint64_t HashBlockOn32Bit(
    uint64_t state, const unsigned char* data, size_t len) {
  // TODO(b/417141985): expose and use CityHash32WithSeed.
  // Note: we can't use PrecombineLengthMix here because len can be up to 1024.
  return CombineRawImpl(
      state + len,
      hash_internal::CityHash32(reinterpret_cast<const char*>(data), len));
}

ABSL_ATTRIBUTE_NOINLINE uint64_t
SplitAndCombineOn32Bit(uint64_t state, const unsigned char* first, size_t len) {
  while (len >= PiecewiseChunkSize()) {
    state = HashBlockOn32Bit(state, first, PiecewiseChunkSize());
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
    uint64_t state, const unsigned char* data, size_t len) {
#ifdef ABSL_HAVE_INTRINSIC_INT128
  return LowLevelHashLenGt32(state, data, len);
#else
  return hash_internal::CityHash64WithSeed(reinterpret_cast<const char*>(data),
                                           len, state);
#endif
}

ABSL_ATTRIBUTE_NOINLINE uint64_t
SplitAndCombineOn64Bit(uint64_t state, const unsigned char* first, size_t len) {
  while (len >= PiecewiseChunkSize()) {
    state = HashBlockOn64Bit(state, first, PiecewiseChunkSize());
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

uint64_t CombineLargeContiguousImplOn32BitLengthGt8(uint64_t state,
                                                    const unsigned char* first,
                                                    size_t len) {
  assert(len > 8);
  assert(sizeof(size_t) == 4);  // NOLINT(misc-static-assert)
  if (ABSL_PREDICT_TRUE(len <= PiecewiseChunkSize())) {
    return HashBlockOn32Bit(state, first, len);
  }
  return SplitAndCombineOn32Bit(state, first, len);
}

uint64_t CombineLargeContiguousImplOn64BitLengthGt32(uint64_t state,
                                                     const unsigned char* first,
                                                     size_t len) {
  assert(len > 32);
  assert(sizeof(size_t) == 8);  // NOLINT(misc-static-assert)
  if (ABSL_PREDICT_TRUE(len <= PiecewiseChunkSize())) {
    return HashBlockOn64Bit(state, first, len);
  }
  return SplitAndCombineOn64Bit(state, first, len);
}

ABSL_CONST_INIT const void* const MixingHashState::kSeed = &kSeed;

}  // namespace hash_internal
ABSL_NAMESPACE_END
}  // namespace absl
