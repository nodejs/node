// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRINGS_STRING_HASHER_INL_H_
#define V8_STRINGS_STRING_HASHER_INL_H_

#include "src/strings/string-hasher.h"

#include "src/objects/objects.h"
#include "src/objects/string-inl.h"
#include "src/strings/char-predicates-inl.h"
#include "src/utils/utils-inl.h"

namespace v8 {
namespace internal {

uint32_t StringHasher::AddCharacterCore(uint32_t running_hash, uint16_t c) {
  running_hash += c;
  running_hash += (running_hash << 10);
  running_hash ^= (running_hash >> 6);
  return running_hash;
}

uint32_t StringHasher::GetHashCore(uint32_t running_hash) {
  running_hash += (running_hash << 3);
  running_hash ^= (running_hash >> 11);
  running_hash += (running_hash << 15);
  int32_t hash = static_cast<int32_t>(running_hash & String::kHashBitMask);
  int32_t mask = (hash - 1) >> 31;
  return running_hash | (kZeroHash & mask);
}

uint32_t StringHasher::GetTrivialHash(int length) {
  DCHECK_GT(length, String::kMaxHashCalcLength);
  // String hash of a large string is simply the length.
  return (static_cast<uint32_t>(length) << String::kHashShift) |
         String::kIsNotArrayIndexMask | String::kIsNotIntegerIndexMask;
}

template <typename schar>
uint32_t StringHasher::HashSequentialString(const schar* chars, int length,
                                            uint64_t seed) {
  DCHECK_LE(0, length);
  DCHECK_IMPLIES(0 < length, chars != nullptr);
  if (length >= 1) {
    if (IsDecimalDigit(chars[0]) && (length == 1 || chars[0] != '0')) {
      uint32_t index = 0;
      if (length <= String::kMaxArrayIndexSize) {
        // Possible array index; try to compute the array index hash.
        index = chars[0] - '0';
        int i = 1;
        do {
          if (i == length) {
            return MakeArrayIndexHash(index, length);
          }
        } while (TryAddIndexChar(&index, chars[i++]));
      }
      // The following block wouldn't do anything on 32-bit platforms,
      // because kMaxArrayIndexSize == kMaxIntegerIndexSize there, and
      // if we wanted to compile it everywhere, then {index_big} would
      // have to be a {size_t}, which the Mac compiler doesn't like to
      // implicitly cast to uint64_t for the {TryAddIndexChar} call.
#if V8_HOST_ARCH_64_BIT
      // No "else" here: if the block above was entered and fell through,
      // we'll have to take this branch.
      if (length <= String::kMaxIntegerIndexSize) {
        // Not an array index, but it could still be an integer index.
        // Perform a regular hash computation, and additionally check
        // if there are non-digit characters.
        uint32_t is_integer_index = 0;
        uint32_t running_hash = static_cast<uint32_t>(seed);
        uint64_t index_big = index;
        const schar* end = &chars[length];
        while (chars != end) {
          if (is_integer_index == 0 && !TryAddIndexChar(&index_big, *chars)) {
            is_integer_index = String::kIsNotIntegerIndexMask;
          }
          running_hash = AddCharacterCore(running_hash, *chars++);
        }
        return (GetHashCore(running_hash) << String::kHashShift) |
               String::kIsNotArrayIndexMask | is_integer_index;
      }
#endif
    }
    // No "else" here: if the first character was a decimal digit, we might
    // still have to take this branch.
    if (length > String::kMaxHashCalcLength) {
      return GetTrivialHash(length);
    }
  }

  // Non-index hash.
  uint32_t running_hash = static_cast<uint32_t>(seed);
  const schar* end = &chars[length];
  while (chars != end) {
    running_hash = AddCharacterCore(running_hash, *chars++);
  }

  return (GetHashCore(running_hash) << String::kHashShift) |
         String::kIsNotArrayIndexMask | String::kIsNotIntegerIndexMask;
}

std::size_t SeededStringHasher::operator()(const char* name) const {
  return StringHasher::HashSequentialString(
      name, static_cast<int>(strlen(name)), hashseed_);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_STRINGS_STRING_HASHER_INL_H_
