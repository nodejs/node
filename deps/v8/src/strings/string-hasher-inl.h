// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRINGS_STRING_HASHER_INL_H_
#define V8_STRINGS_STRING_HASHER_INL_H_

#include "src/strings/string-hasher.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/globals.h"
#include "src/utils/utils.h"

#ifdef __SSE2__
#include <emmintrin.h>
#elif defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

// Comment inserted to prevent header reordering.
#include <type_traits>

#include "src/objects/name-inl.h"
#include "src/objects/string-inl.h"
#include "src/strings/char-predicates-inl.h"
#include "src/utils/utils-inl.h"
#include "third_party/rapidhash-v8/rapidhash.h"

namespace v8 {
namespace internal {

namespace detail {
V8_EXPORT_PRIVATE uint64_t HashConvertingTo8Bit(const uint16_t* chars,
                                                uint32_t length, uint64_t seed);

template <typename T>
uint32_t ConvertRawHashToUsableHash(T raw_hash) {
  // Limit to the supported bits.
  const int32_t hash = static_cast<int32_t>(raw_hash & String::HashBits::kMax);
  // Ensure that the hash is kZeroHash, if the computed value is 0.
  return hash == 0 ? StringHasher::kZeroHash : hash;
}

V8_INLINE bool IsOnly8Bit(const uint16_t* chars, unsigned len) {
  // TODO(leszeks): This could be SIMD for efficiency on large strings, if we
  // need it.
  for (unsigned i = 0; i < len; ++i) {
    if (chars[i] > 255) {
      return false;
    }
  }
  return true;
}

V8_INLINE uint64_t GetRapidHash(const uint8_t* chars, uint32_t length,
                                uint64_t seed) {
  return rapidhash(chars, length, seed);
}

V8_INLINE uint64_t GetRapidHash(const uint16_t* chars, uint32_t length,
                                uint64_t seed) {
  // For 2-byte strings we need to preserve the same hash for strings in just
  // the latin-1 range.
  if (V8_UNLIKELY(IsOnly8Bit(chars, length))) {
    return detail::HashConvertingTo8Bit(chars, length, seed);
  }
  return rapidhash(reinterpret_cast<const uint8_t*>(chars), 2 * length, seed);
}

template <typename uchar>
V8_INLINE uint32_t GetUsableRapidHash(const uchar* chars, uint32_t length,
                                      uint64_t seed) {
  return ConvertRawHashToUsableHash(GetRapidHash(chars, length, seed));
}
}  // namespace detail

void RunningStringHasher::AddCharacter(uint16_t c) {
  running_hash_ += c;
  running_hash_ += (running_hash_ << 10);
  running_hash_ ^= (running_hash_ >> 6);
}

uint32_t RunningStringHasher::Finalize() {
  running_hash_ += (running_hash_ << 3);
  running_hash_ ^= (running_hash_ >> 11);
  running_hash_ += (running_hash_ << 15);
  return detail::ConvertRawHashToUsableHash(running_hash_);
}

uint32_t StringHasher::GetTrivialHash(uint32_t length) {
  DCHECK_GT(length, String::kMaxHashCalcLength);
  // The hash of a large string is simply computed from the length.
  // Ensure that the max length is small enough to be encoded without losing
  // information.
  static_assert(String::kMaxLength <= String::HashBits::kMax);
  uint32_t hash = length;
  return String::CreateHashFieldValue(hash, String::HashFieldType::kHash);
}

uint32_t StringHasher::MakeArrayIndexHash(uint32_t value, uint32_t length) {
  // For array indexes mix the length into the hash as an array index could
  // be zero.
  DCHECK_LE(length, String::kMaxArrayIndexSize);
  DCHECK(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));

  value <<= String::ArrayIndexValueBits::kShift;
  value |= length << String::ArrayIndexLengthBits::kShift;

  DCHECK(String::IsIntegerIndex(value));
  DCHECK_EQ(length <= String::kMaxCachedArrayIndexLength,
            Name::ContainsCachedArrayIndex(value));
  return value;
}

namespace detail {

enum IndexParseResult { kSuccess, kNonIndex, kOverflow };

// The array index type depends on the architecture, so that the multiplication
// in TryParseArrayIndex stays fast.
#if V8_HOST_ARCH_64_BIT
using ArrayIndexT = uint64_t;
#else
using ArrayIndexT = uint32_t;
#endif

template <typename uchar>
V8_INLINE IndexParseResult TryParseArrayIndex(const uchar* chars,
                                              uint32_t length, uint32_t& i,
                                              ArrayIndexT& index) {
  DCHECK_GT(length, 0);
  DCHECK_LE(length, String::kMaxIntegerIndexSize);

  // The leading character can only be a zero for the string "0"; otherwise this
  // isn't a valid index string.
  index = chars[0] - '0';
  i = 1;
  if (index > 9) return kNonIndex;
  if (index == 0) {
    if (length > 1) return kNonIndex;
    DCHECK_EQ(length, 1);
    return kSuccess;
  }

  if (length > String::kMaxArrayIndexSize) return kOverflow;

  // TODO(leszeks): Use SIMD for this loop.
  for (; i < length; i++) {
    uchar c = chars[i];
    uint32_t val = c - '0';
    if (val > 9) return kNonIndex;
    index = (10 * index) + val;
  }
  if constexpr (sizeof(index) == sizeof(uint64_t)) {
    // If we have a large type for index, we'll never overflow it, so we can
    // have a simple comparison for array index overflow.
    if (index > String::kMaxArrayIndex) {
      return kOverflow;
    }
  } else {
    DCHECK(sizeof(index) == sizeof(uint32_t));
    // If index is a 32-bit int, we have to get a bit creative with the overflow
    // check.
    if (V8_UNLIKELY(length == String::kMaxArrayIndexSize)) {
      // If length is String::kMaxArrayIndexSize, and we know there is no zero
      // prefix, the minimum valid value is 1 followed by length - 1 zeros. If
      // our value is smaller than this, then we overflowed.
      //
      // Additionally, String::kMaxArrayIndex is UInt32Max - 1, so we can fold
      // in a check that index < UInt32Max by adding 1 to both sides, making
      // index = UInt32Max overflows, and only then checking for overflow.
      constexpr uint32_t kMinValidValue =
          TenToThe(String::kMaxArrayIndexSize - 1);
      if (index + 1 < kMinValidValue + 1) {
        // We won't try an integer index if there is overflow, so just return
        // non-index.
        DCHECK(String::kMaxArrayIndexSize == String::kMaxIntegerIndexSize);
        return kNonIndex;
      }
    }
  }
  DCHECK_LT(index, TenToThe(length));
  DCHECK_GE(index, TenToThe(length - 1));
  return kSuccess;
}

// The following function wouldn't do anything on 32-bit platforms, because
// kMaxArrayIndexSize == kMaxIntegerIndexSize there.
#if V8_HOST_ARCH_64_BIT
template <typename uchar>
V8_INLINE IndexParseResult TryParseIntegerIndex(const uchar* chars,
                                                uint32_t length, uint32_t i,
                                                ArrayIndexT index) {
  DCHECK_GT(length, 0);
  DCHECK_LE(length, String::kMaxIntegerIndexSize);
  DCHECK_GT(i, 0);
  DCHECK_GT(index, 0);
  DCHECK_LT(index, kMaxSafeIntegerUint64);

  for (; i < length; i++) {
    // We should never be anywhere near overflowing, so we can just do
    // one range check at the end.
    static_assert(kMaxSafeIntegerUint64 < (kMaxUInt64 / 100));
    DCHECK_LT(index, kMaxUInt64 / 100);

    uchar c = chars[i];
    uint32_t val = c - '0';
    if (val > 9) return kNonIndex;
    index = (10 * index) + val;
  }
  if (index > kMaxSafeIntegerUint64) return kOverflow;

  return kSuccess;
}
#else
static_assert(String::kMaxArrayIndexSize == String::kMaxIntegerIndexSize);
#endif

}  // namespace detail

template <typename char_t>
uint32_t StringHasher::HashSequentialString(const char_t* chars_raw,
                                            uint32_t length, uint64_t seed) {
  static_assert(std::is_integral_v<char_t>);
  static_assert(sizeof(char_t) <= 2);
  using uchar = std::make_unsigned_t<char_t>;
  const uchar* chars = reinterpret_cast<const uchar*>(chars_raw);
  DCHECK_IMPLIES(length > 0, chars != nullptr);
  if (length >= 1) {
    if (length <= String::kMaxIntegerIndexSize) {
      // Possible array or integer index; try to compute the array index hash.
      static_assert(String::kMaxArrayIndexSize <= String::kMaxIntegerIndexSize);

      detail::ArrayIndexT index;
      uint32_t i;
      switch (detail::TryParseArrayIndex(chars, length, i, index)) {
        case detail::kSuccess:
          DCHECK_LE(index, String::kMaxArrayIndex);
          return MakeArrayIndexHash(static_cast<uint32_t>(index), length);
        case detail::kNonIndex:
          // A non-index result from TryParseArrayIndex means we don't need to
          // check for integer indices.
          break;
        case detail::kOverflow: {
#if V8_HOST_ARCH_64_BIT
          // On 64-bit, we might have a valid integer index even if the value
          // overflowed an array index.
          static_assert(String::kMaxArrayIndexSize <
                        String::kMaxIntegerIndexSize);
          switch (detail::TryParseIntegerIndex(chars, length, i, index)) {
            case detail::kSuccess: {
              uint32_t hash = String::CreateHashFieldValue(
                  detail::GetUsableRapidHash(chars, length, seed),
                  String::HashFieldType::kIntegerIndex);
              if (Name::ContainsCachedArrayIndex(hash)) {
                // The hash accidentally looks like a cached index. Fix that by
                // setting a bit that looks like a longer-than-cacheable string
                // length.
                hash |= (String::kMaxCachedArrayIndexLength + 1)
                        << String::ArrayIndexLengthBits::kShift;
              }
              DCHECK(!Name::ContainsCachedArrayIndex(hash));
              return hash;
            }
            case detail::kNonIndex:
            case detail::kOverflow:
              break;
          }
#else
          static_assert(String::kMaxArrayIndexSize ==
                        String::kMaxIntegerIndexSize);
#endif
          break;
        }
      }
      // If the we failed to compute an index hash, this falls through into the
      // non-index hash case.
    } else if (length > String::kMaxHashCalcLength) {
      // We should never go down this path if we might have an index value.
      static_assert(String::kMaxHashCalcLength > String::kMaxIntegerIndexSize);
      static_assert(String::kMaxHashCalcLength > String::kMaxArrayIndexSize);
      return GetTrivialHash(length);
    }
  }

  // Non-index hash.
  return String::CreateHashFieldValue(
      detail::GetUsableRapidHash(chars, length, seed),
      String::HashFieldType::kHash);
}

std::size_t SeededStringHasher::operator()(const char* name) const {
  return StringHasher::HashSequentialString(
      name, static_cast<uint32_t>(strlen(name)), hashseed_);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_STRINGS_STRING_HASHER_INL_H_
