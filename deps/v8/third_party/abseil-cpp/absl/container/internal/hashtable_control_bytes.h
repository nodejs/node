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
// This file contains the implementation of the hashtable control bytes
// manipulation.

#ifndef ABSL_CONTAINER_INTERNAL_HASHTABLE_CONTROL_BYTES_H_
#define ABSL_CONTAINER_INTERNAL_HASHTABLE_CONTROL_BYTES_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "absl/base/config.h"

#ifdef ABSL_INTERNAL_HAVE_SSE2
#include <emmintrin.h>
#endif

#ifdef ABSL_INTERNAL_HAVE_SSSE3
#include <tmmintrin.h>
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef ABSL_INTERNAL_HAVE_ARM_NEON
#include <arm_neon.h>
#endif

#include "absl/base/optimization.h"
#include "absl/numeric/bits.h"
#include "absl/base/internal/endian.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {

#ifdef ABSL_SWISSTABLE_ASSERT
#error ABSL_SWISSTABLE_ASSERT cannot be directly set
#else
// We use this macro for assertions that users may see when the table is in an
// invalid state that sanitizers may help diagnose.
#define ABSL_SWISSTABLE_ASSERT(CONDITION) \
  assert((CONDITION) && "Try enabling sanitizers.")
#endif


template <typename T>
uint32_t TrailingZeros(T x) {
  ABSL_ASSUME(x != 0);
  return static_cast<uint32_t>(countr_zero(x));
}

// 8 bytes bitmask with most significant bit set for every byte.
constexpr uint64_t kMsbs8Bytes = 0x8080808080808080ULL;
// 8 kEmpty bytes that is useful for small table initialization.
constexpr uint64_t k8EmptyBytes = kMsbs8Bytes;

// An abstract bitmask, such as that emitted by a SIMD instruction.
//
// Specifically, this type implements a simple bitset whose representation is
// controlled by `SignificantBits` and `Shift`. `SignificantBits` is the number
// of abstract bits in the bitset, while `Shift` is the log-base-two of the
// width of an abstract bit in the representation.
// This mask provides operations for any number of real bits set in an abstract
// bit. To add iteration on top of that, implementation must guarantee no more
// than the most significant real bit is set in a set abstract bit.
template <class T, int SignificantBits, int Shift = 0>
class NonIterableBitMask {
 public:
  explicit NonIterableBitMask(T mask) : mask_(mask) {}

  explicit operator bool() const { return this->mask_ != 0; }

  // Returns the index of the lowest *abstract* bit set in `self`.
  uint32_t LowestBitSet() const {
    return container_internal::TrailingZeros(mask_) >> Shift;
  }

  // Returns the index of the highest *abstract* bit set in `self`.
  uint32_t HighestBitSet() const {
    return static_cast<uint32_t>((bit_width(mask_) - 1) >> Shift);
  }

  // Returns the number of trailing zero *abstract* bits.
  uint32_t TrailingZeros() const {
    return container_internal::TrailingZeros(mask_) >> Shift;
  }

  // Returns the number of leading zero *abstract* bits.
  uint32_t LeadingZeros() const {
    constexpr int total_significant_bits = SignificantBits << Shift;
    constexpr int extra_bits = sizeof(T) * 8 - total_significant_bits;
    return static_cast<uint32_t>(
               countl_zero(static_cast<T>(mask_ << extra_bits))) >>
           Shift;
  }

  T mask_;
};

// Mask that can be iterable
//
// For example, when `SignificantBits` is 16 and `Shift` is zero, this is just
// an ordinary 16-bit bitset occupying the low 16 bits of `mask`. When
// `SignificantBits` is 8 and `Shift` is 3, abstract bits are represented as
// the bytes `0x00` and `0x80`, and it occupies all 64 bits of the bitmask.
// If NullifyBitsOnIteration is true (only allowed for Shift == 3),
// non zero abstract bit is allowed to have additional bits
// (e.g., `0xff`, `0x83` and `0x9c` are ok, but `0x6f` is not).
//
// For example:
//   for (int i : BitMask<uint32_t, 16>(0b101)) -> yields 0, 2
//   for (int i : BitMask<uint64_t, 8, 3>(0x0000000080800000)) -> yields 2, 3
template <class T, int SignificantBits, int Shift = 0,
          bool NullifyBitsOnIteration = false>
class BitMask : public NonIterableBitMask<T, SignificantBits, Shift> {
  using Base = NonIterableBitMask<T, SignificantBits, Shift>;
  static_assert(std::is_unsigned<T>::value, "");
  static_assert(Shift == 0 || Shift == 3, "");
  static_assert(!NullifyBitsOnIteration || Shift == 3, "");

 public:
  explicit BitMask(T mask) : Base(mask) {
    if (Shift == 3 && !NullifyBitsOnIteration) {
      ABSL_SWISSTABLE_ASSERT(this->mask_ == (this->mask_ & kMsbs8Bytes));
    }
  }
  // BitMask is an iterator over the indices of its abstract bits.
  using value_type = int;
  using iterator = BitMask;
  using const_iterator = BitMask;

  BitMask& operator++() {
    if (Shift == 3 && NullifyBitsOnIteration) {
      this->mask_ &= kMsbs8Bytes;
    }
    this->mask_ &= (this->mask_ - 1);
    return *this;
  }

  uint32_t operator*() const { return Base::LowestBitSet(); }

  BitMask begin() const { return *this; }
  BitMask end() const { return BitMask(0); }

 private:
  friend bool operator==(const BitMask& a, const BitMask& b) {
    return a.mask_ == b.mask_;
  }
  friend bool operator!=(const BitMask& a, const BitMask& b) {
    return a.mask_ != b.mask_;
  }
};

using h2_t = uint8_t;

// The values here are selected for maximum performance. See the static asserts
// below for details.

// A `ctrl_t` is a single control byte, which can have one of four
// states: empty, deleted, full (which has an associated seven-bit h2_t value)
// and the sentinel. They have the following bit patterns:
//
//      empty: 1 0 0 0 0 0 0 0
//    deleted: 1 1 1 1 1 1 1 0
//       full: 0 h h h h h h h  // h represents the hash bits.
//   sentinel: 1 1 1 1 1 1 1 1
//
// These values are specifically tuned for SSE-flavored SIMD.
// The static_asserts below detail the source of these choices.
//
// We use an enum class so that when strict aliasing is enabled, the compiler
// knows ctrl_t doesn't alias other types.
enum class ctrl_t : int8_t {
  kEmpty = -128,   // 0b10000000
  kDeleted = -2,   // 0b11111110
  kSentinel = -1,  // 0b11111111
};
static_assert(
    (static_cast<int8_t>(ctrl_t::kEmpty) &
     static_cast<int8_t>(ctrl_t::kDeleted) &
     static_cast<int8_t>(ctrl_t::kSentinel) & 0x80) != 0,
    "Special markers need to have the MSB to make checking for them efficient");
static_assert(
    ctrl_t::kEmpty < ctrl_t::kSentinel && ctrl_t::kDeleted < ctrl_t::kSentinel,
    "ctrl_t::kEmpty and ctrl_t::kDeleted must be smaller than "
    "ctrl_t::kSentinel to make the SIMD test of IsEmptyOrDeleted() efficient");
static_assert(
    ctrl_t::kSentinel == static_cast<ctrl_t>(-1),
    "ctrl_t::kSentinel must be -1 to elide loading it from memory into SIMD "
    "registers (pcmpeqd xmm, xmm)");
static_assert(ctrl_t::kEmpty == static_cast<ctrl_t>(-128),
              "ctrl_t::kEmpty must be -128 to make the SIMD check for its "
              "existence efficient (psignb xmm, xmm)");
static_assert(
    (~static_cast<int8_t>(ctrl_t::kEmpty) &
     ~static_cast<int8_t>(ctrl_t::kDeleted) &
     static_cast<int8_t>(ctrl_t::kSentinel) & 0x7F) != 0,
    "ctrl_t::kEmpty and ctrl_t::kDeleted must share an unset bit that is not "
    "shared by ctrl_t::kSentinel to make the scalar test for "
    "MaskEmptyOrDeleted() efficient");
static_assert(ctrl_t::kDeleted == static_cast<ctrl_t>(-2),
              "ctrl_t::kDeleted must be -2 to make the implementation of "
              "ConvertSpecialToEmptyAndFullToDeleted efficient");

// Helpers for checking the state of a control byte.
inline bool IsEmpty(ctrl_t c) { return c == ctrl_t::kEmpty; }
inline bool IsFull(ctrl_t c) {
  // Cast `c` to the underlying type instead of casting `0` to `ctrl_t` as `0`
  // is not a value in the enum. Both ways are equivalent, but this way makes
  // linters happier.
  return static_cast<std::underlying_type_t<ctrl_t>>(c) >= 0;
}
inline bool IsDeleted(ctrl_t c) { return c == ctrl_t::kDeleted; }
inline bool IsEmptyOrDeleted(ctrl_t c) { return c < ctrl_t::kSentinel; }

#ifdef ABSL_INTERNAL_HAVE_SSE2
// Quick reference guide for intrinsics used below:
//
// * __m128i: An XMM (128-bit) word.
//
// * _mm_setzero_si128: Returns a zero vector.
// * _mm_set1_epi8:     Returns a vector with the same i8 in each lane.
//
// * _mm_subs_epi8:    Saturating-subtracts two i8 vectors.
// * _mm_and_si128:    Ands two i128s together.
// * _mm_or_si128:     Ors two i128s together.
// * _mm_andnot_si128: And-nots two i128s together.
//
// * _mm_cmpeq_epi8: Component-wise compares two i8 vectors for equality,
//                   filling each lane with 0x00 or 0xff.
// * _mm_cmpgt_epi8: Same as above, but using > rather than ==.
//
// * _mm_loadu_si128:  Performs an unaligned load of an i128.
// * _mm_storeu_si128: Performs an unaligned store of an i128.
//
// * _mm_sign_epi8:     Retains, negates, or zeroes each i8 lane of the first
//                      argument if the corresponding lane of the second
//                      argument is positive, negative, or zero, respectively.
// * _mm_movemask_epi8: Selects the sign bit out of each i8 lane and produces a
//                      bitmask consisting of those bits.
// * _mm_shuffle_epi8:  Selects i8s from the first argument, using the low
//                      four bits of each i8 lane in the second argument as
//                      indices.

// https://github.com/abseil/abseil-cpp/issues/209
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=87853
// _mm_cmpgt_epi8 is broken under GCC with -funsigned-char
// Work around this by using the portable implementation of Group
// when using -funsigned-char under GCC.
inline __m128i _mm_cmpgt_epi8_fixed(__m128i a, __m128i b) {
#if defined(__GNUC__) && !defined(__clang__)
  if (std::is_unsigned<char>::value) {
    const __m128i mask = _mm_set1_epi8(0x80);
    const __m128i diff = _mm_subs_epi8(b, a);
    return _mm_cmpeq_epi8(_mm_and_si128(diff, mask), mask);
  }
#endif
  return _mm_cmpgt_epi8(a, b);
}

struct GroupSse2Impl {
  static constexpr size_t kWidth = 16;  // the number of slots per group
  using BitMaskType = BitMask<uint16_t, kWidth>;
  using NonIterableBitMaskType = NonIterableBitMask<uint16_t, kWidth>;

  explicit GroupSse2Impl(const ctrl_t* pos) {
    ctrl = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pos));
  }

  // Returns a bitmask representing the positions of slots that match hash.
  BitMaskType Match(h2_t hash) const {
    auto match = _mm_set1_epi8(static_cast<char>(hash));
    return BitMaskType(
        static_cast<uint16_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(match, ctrl))));
  }

  // Returns a bitmask representing the positions of empty slots.
  NonIterableBitMaskType MaskEmpty() const {
#ifdef ABSL_INTERNAL_HAVE_SSSE3
    // This only works because ctrl_t::kEmpty is -128.
    return NonIterableBitMaskType(
        static_cast<uint16_t>(_mm_movemask_epi8(_mm_sign_epi8(ctrl, ctrl))));
#else
    auto match = _mm_set1_epi8(static_cast<char>(ctrl_t::kEmpty));
    return NonIterableBitMaskType(
        static_cast<uint16_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(match, ctrl))));
#endif
  }

  // Returns a bitmask representing the positions of full slots.
  // Note: for `is_small()` tables group may contain the "same" slot twice:
  // original and mirrored.
  BitMaskType MaskFull() const {
    return BitMaskType(static_cast<uint16_t>(_mm_movemask_epi8(ctrl) ^ 0xffff));
  }

  // Returns a bitmask representing the positions of non full slots.
  // Note: this includes: kEmpty, kDeleted, kSentinel.
  // It is useful in contexts when kSentinel is not present.
  auto MaskNonFull() const {
    return BitMaskType(static_cast<uint16_t>(_mm_movemask_epi8(ctrl)));
  }

  // Returns a bitmask representing the positions of empty or deleted slots.
  NonIterableBitMaskType MaskEmptyOrDeleted() const {
    auto special = _mm_set1_epi8(static_cast<char>(ctrl_t::kSentinel));
    return NonIterableBitMaskType(static_cast<uint16_t>(
        _mm_movemask_epi8(_mm_cmpgt_epi8_fixed(special, ctrl))));
  }

  // Returns the number of trailing empty or deleted elements in the group.
  uint32_t CountLeadingEmptyOrDeleted() const {
    auto special = _mm_set1_epi8(static_cast<char>(ctrl_t::kSentinel));
    return TrailingZeros(static_cast<uint32_t>(
        _mm_movemask_epi8(_mm_cmpgt_epi8_fixed(special, ctrl)) + 1));
  }

  void ConvertSpecialToEmptyAndFullToDeleted(ctrl_t* dst) const {
    auto msbs = _mm_set1_epi8(static_cast<char>(-128));
    auto x126 = _mm_set1_epi8(126);
#ifdef ABSL_INTERNAL_HAVE_SSSE3
    auto res = _mm_or_si128(_mm_shuffle_epi8(x126, ctrl), msbs);
#else
    auto zero = _mm_setzero_si128();
    auto special_mask = _mm_cmpgt_epi8_fixed(zero, ctrl);
    auto res = _mm_or_si128(msbs, _mm_andnot_si128(special_mask, x126));
#endif
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), res);
  }

  __m128i ctrl;
};
#endif  // ABSL_INTERNAL_RAW_HASH_SET_HAVE_SSE2

#if defined(ABSL_INTERNAL_HAVE_ARM_NEON) && defined(ABSL_IS_LITTLE_ENDIAN)
struct GroupAArch64Impl {
  static constexpr size_t kWidth = 8;
  using BitMaskType = BitMask<uint64_t, kWidth, /*Shift=*/3,
                              /*NullifyBitsOnIteration=*/true>;
  using NonIterableBitMaskType =
      NonIterableBitMask<uint64_t, kWidth, /*Shift=*/3>;

  explicit GroupAArch64Impl(const ctrl_t* pos) {
    ctrl = vld1_u8(reinterpret_cast<const uint8_t*>(pos));
  }

  auto Match(h2_t hash) const {
    uint8x8_t dup = vdup_n_u8(hash);
    auto mask = vceq_u8(ctrl, dup);
    return BitMaskType(vget_lane_u64(vreinterpret_u64_u8(mask), 0));
  }

  auto MaskEmpty() const {
    uint64_t mask =
        vget_lane_u64(vreinterpret_u64_u8(vceq_s8(
                          vdup_n_s8(static_cast<int8_t>(ctrl_t::kEmpty)),
                          vreinterpret_s8_u8(ctrl))),
                      0);
    return NonIterableBitMaskType(mask);
  }

  // Returns a bitmask representing the positions of full slots.
  // Note: for `is_small()` tables group may contain the "same" slot twice:
  // original and mirrored.
  auto MaskFull() const {
    uint64_t mask = vget_lane_u64(
        vreinterpret_u64_u8(vcge_s8(vreinterpret_s8_u8(ctrl),
                                    vdup_n_s8(static_cast<int8_t>(0)))),
        0);
    return BitMaskType(mask);
  }

  // Returns a bitmask representing the positions of non full slots.
  // Note: this includes: kEmpty, kDeleted, kSentinel.
  // It is useful in contexts when kSentinel is not present.
  auto MaskNonFull() const {
    uint64_t mask = vget_lane_u64(
        vreinterpret_u64_u8(vclt_s8(vreinterpret_s8_u8(ctrl),
                                    vdup_n_s8(static_cast<int8_t>(0)))),
        0);
    return BitMaskType(mask);
  }

  auto MaskEmptyOrDeleted() const {
    uint64_t mask =
        vget_lane_u64(vreinterpret_u64_u8(vcgt_s8(
                          vdup_n_s8(static_cast<int8_t>(ctrl_t::kSentinel)),
                          vreinterpret_s8_u8(ctrl))),
                      0);
    return NonIterableBitMaskType(mask);
  }

  uint32_t CountLeadingEmptyOrDeleted() const {
    uint64_t mask =
        vget_lane_u64(vreinterpret_u64_u8(vcle_s8(
                          vdup_n_s8(static_cast<int8_t>(ctrl_t::kSentinel)),
                          vreinterpret_s8_u8(ctrl))),
                      0);
    // Similar to MaskEmptyorDeleted() but we invert the logic to invert the
    // produced bitfield. We then count number of trailing zeros.
    // Clang and GCC optimize countr_zero to rbit+clz without any check for 0,
    // so we should be fine.
    return static_cast<uint32_t>(countr_zero(mask)) >> 3;
  }

  void ConvertSpecialToEmptyAndFullToDeleted(ctrl_t* dst) const {
    uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(ctrl), 0);
    constexpr uint64_t slsbs = 0x0202020202020202ULL;
    constexpr uint64_t midbs = 0x7e7e7e7e7e7e7e7eULL;
    auto x = slsbs & (mask >> 6);
    auto res = (x + midbs) | kMsbs8Bytes;
    little_endian::Store64(dst, res);
  }

  uint8x8_t ctrl;
};
#endif  // ABSL_INTERNAL_HAVE_ARM_NEON && ABSL_IS_LITTLE_ENDIAN

struct GroupPortableImpl {
  static constexpr size_t kWidth = 8;
  using BitMaskType = BitMask<uint64_t, kWidth, /*Shift=*/3,
                              /*NullifyBitsOnIteration=*/false>;
  using NonIterableBitMaskType =
      NonIterableBitMask<uint64_t, kWidth, /*Shift=*/3>;

  explicit GroupPortableImpl(const ctrl_t* pos)
      : ctrl(little_endian::Load64(pos)) {}

  BitMaskType Match(h2_t hash) const {
    // For the technique, see:
    // http://graphics.stanford.edu/~seander/bithacks.html##ValueInWord
    // (Determine if a word has a byte equal to n).
    //
    // Caveat: there are false positives but:
    // - they only occur if there is a real match
    // - they never occur on ctrl_t::kEmpty, ctrl_t::kDeleted, ctrl_t::kSentinel
    // - they will be handled gracefully by subsequent checks in code
    //
    // Example:
    //   v = 0x1716151413121110
    //   hash = 0x12
    //   retval = (v - lsbs) & ~v & msbs = 0x0000000080800000
    constexpr uint64_t lsbs = 0x0101010101010101ULL;
    auto x = ctrl ^ (lsbs * hash);
    return BitMaskType((x - lsbs) & ~x & kMsbs8Bytes);
  }

  auto MaskEmpty() const {
    return NonIterableBitMaskType((ctrl & ~(ctrl << 6)) & kMsbs8Bytes);
  }

  // Returns a bitmask representing the positions of full slots.
  // Note: for `is_small()` tables group may contain the "same" slot twice:
  // original and mirrored.
  auto MaskFull() const {
    return BitMaskType((ctrl ^ kMsbs8Bytes) & kMsbs8Bytes);
  }

  // Returns a bitmask representing the positions of non full slots.
  // Note: this includes: kEmpty, kDeleted, kSentinel.
  // It is useful in contexts when kSentinel is not present.
  auto MaskNonFull() const { return BitMaskType(ctrl & kMsbs8Bytes); }

  auto MaskEmptyOrDeleted() const {
    return NonIterableBitMaskType((ctrl & ~(ctrl << 7)) & kMsbs8Bytes);
  }

  uint32_t CountLeadingEmptyOrDeleted() const {
    // ctrl | ~(ctrl >> 7) will have the lowest bit set to zero for kEmpty and
    // kDeleted. We lower all other bits and count number of trailing zeros.
    constexpr uint64_t bits = 0x0101010101010101ULL;
    return static_cast<uint32_t>(countr_zero((ctrl | ~(ctrl >> 7)) & bits) >>
                                 3);
  }

  void ConvertSpecialToEmptyAndFullToDeleted(ctrl_t* dst) const {
    constexpr uint64_t lsbs = 0x0101010101010101ULL;
    auto x = ctrl & kMsbs8Bytes;
    auto res = (~x + (x >> 7)) & ~lsbs;
    little_endian::Store64(dst, res);
  }

  uint64_t ctrl;
};

#ifdef ABSL_INTERNAL_HAVE_SSE2
using Group = GroupSse2Impl;
using GroupFullEmptyOrDeleted = GroupSse2Impl;
#elif defined(ABSL_INTERNAL_HAVE_ARM_NEON) && defined(ABSL_IS_LITTLE_ENDIAN)
using Group = GroupAArch64Impl;
// For Aarch64, we use the portable implementation for counting and masking
// full, empty or deleted group elements. This is to avoid the latency of moving
// between data GPRs and Neon registers when it does not provide a benefit.
// Using Neon is profitable when we call Match(), but is not when we don't,
// which is the case when we do *EmptyOrDeleted and MaskFull operations.
// It is difficult to make a similar approach beneficial on other architectures
// such as x86 since they have much lower GPR <-> vector register transfer
// latency and 16-wide Groups.
using GroupFullEmptyOrDeleted = GroupPortableImpl;
#else
using Group = GroupPortableImpl;
using GroupFullEmptyOrDeleted = GroupPortableImpl;
#endif

}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl

#undef ABSL_SWISSTABLE_ASSERT

#endif  // ABSL_CONTAINER_INTERNAL_HASHTABLE_CONTROL_BYTES_H_
