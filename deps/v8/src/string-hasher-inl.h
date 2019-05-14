// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRING_HASHER_INL_H_
#define V8_STRING_HASHER_INL_H_

#include "src/string-hasher.h"

#include "src/char-predicates-inl.h"
#include "src/objects.h"
#include "src/objects/string-inl.h"
#include "src/utils-inl.h"

namespace v8 {
namespace internal {

StringHasher::StringHasher(int length, uint64_t seed)
    : length_(length),
      raw_running_hash_(static_cast<uint32_t>(seed)),
      array_index_(0),
      is_array_index_(IsInRange(length, 1, String::kMaxArrayIndexSize)) {
  DCHECK(FLAG_randomize_hashes || raw_running_hash_ == 0);
}

bool StringHasher::has_trivial_hash() {
  return length_ > String::kMaxHashCalcLength;
}

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

template <typename Char>
uint32_t StringHasher::ComputeRunningHash(uint32_t running_hash,
                                          const Char* chars, int length) {
  DCHECK_LE(0, length);
  DCHECK_IMPLIES(0 < length, chars != nullptr);
  const Char* end = &chars[length];
  while (chars != end) {
    running_hash = AddCharacterCore(running_hash, *chars++);
  }
  return running_hash;
}

void StringHasher::AddCharacter(uint16_t c) {
  // Use the Jenkins one-at-a-time hash function to update the hash
  // for the given character.
  raw_running_hash_ = AddCharacterCore(raw_running_hash_, c);
}

bool StringHasher::UpdateIndex(uint16_t c) {
  DCHECK(is_array_index_);
  if (!TryAddIndexChar(&array_index_, c)) {
    is_array_index_ = false;
    return false;
  }
  is_array_index_ = array_index_ != 0 || length_ == 1;
  return is_array_index_;
}

template <typename Char>
inline void StringHasher::AddCharacters(const Char* chars, int length) {
  DCHECK(sizeof(Char) == 1 || sizeof(Char) == 2);
  int i = 0;
  if (is_array_index_) {
    for (; i < length; i++) {
      AddCharacter(chars[i]);
      if (!UpdateIndex(chars[i])) {
        i++;
        break;
      }
    }
  }
  raw_running_hash_ =
      ComputeRunningHash(raw_running_hash_, &chars[i], length - i);
}

template <typename schar>
uint32_t StringHasher::HashSequentialString(const schar* chars, int length,
                                            uint64_t seed) {
#ifdef DEBUG
  StringHasher hasher(length, seed);
  if (!hasher.has_trivial_hash()) hasher.AddCharacters(chars, length);
  uint32_t expected = hasher.GetHashField();
#endif

  // Check whether the string is a valid array index. In that case, compute the
  // array index hash. It'll fall through to compute a regular string hash from
  // the start if it turns out that the string isn't a valid array index.
  if (IsInRange(length, 1, String::kMaxArrayIndexSize)) {
    if (IsDecimalDigit(chars[0]) && (length == 1 || chars[0] != '0')) {
      uint32_t index = chars[0] - '0';
      int i = 1;
      do {
        if (i == length) {
          uint32_t result = MakeArrayIndexHash(index, length);
          DCHECK_EQ(expected, result);
          return result;
        }
      } while (TryAddIndexChar(&index, chars[i++]));
    }
  } else if (length > String::kMaxHashCalcLength) {
    // String hash of a large string is simply the length.
    uint32_t result =
        (length << String::kHashShift) | String::kIsNotArrayIndexMask;
    DCHECK_EQ(result, expected);
    return result;
  }

  // Non-array-index hash.
  uint32_t hash =
      ComputeRunningHash(static_cast<uint32_t>(seed), chars, length);

  uint32_t result =
      (GetHashCore(hash) << String::kHashShift) | String::kIsNotArrayIndexMask;
  DCHECK_EQ(result, expected);
  return result;
}

IteratingStringHasher::IteratingStringHasher(int len, uint64_t seed)
    : StringHasher(len, seed) {}

uint32_t IteratingStringHasher::Hash(String string, uint64_t seed) {
  IteratingStringHasher hasher(string->length(), seed);
  // Nothing to do.
  if (hasher.has_trivial_hash()) return hasher.GetHashField();
  ConsString cons_string = String::VisitFlat(&hasher, string);
  if (cons_string.is_null()) return hasher.GetHashField();
  hasher.VisitConsString(cons_string);
  return hasher.GetHashField();
}

void IteratingStringHasher::VisitOneByteString(const uint8_t* chars,
                                               int length) {
  AddCharacters(chars, length);
}

void IteratingStringHasher::VisitTwoByteString(const uint16_t* chars,
                                               int length) {
  AddCharacters(chars, length);
}

std::size_t SeededStringHasher::operator()(const char* name) const {
  return StringHasher::HashSequentialString(
      name, static_cast<int>(strlen(name)), hashseed_);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_STRING_HASHER_INL_H_
