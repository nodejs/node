// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRING_HASHER_INL_H_
#define V8_STRING_HASHER_INL_H_

#include "src/objects.h"
#include "src/string-hasher.h"

namespace v8 {
namespace internal {

StringHasher::StringHasher(int length, uint32_t seed)
    : length_(length),
      raw_running_hash_(seed),
      array_index_(0),
      is_array_index_(0 < length_ && length_ <= String::kMaxArrayIndexSize),
      is_first_char_(true) {
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
  if ((running_hash & String::kHashBitMask) == 0) {
    return kZeroHash;
  }
  return running_hash;
}

uint32_t StringHasher::ComputeRunningHash(uint32_t running_hash,
                                          const uc16* chars, int length) {
  DCHECK_NOT_NULL(chars);
  DCHECK(length >= 0);
  for (int i = 0; i < length; ++i) {
    running_hash = AddCharacterCore(running_hash, *chars++);
  }
  return running_hash;
}

uint32_t StringHasher::ComputeRunningHashOneByte(uint32_t running_hash,
                                                 const char* chars,
                                                 int length) {
  DCHECK_NOT_NULL(chars);
  DCHECK(length >= 0);
  for (int i = 0; i < length; ++i) {
    uint16_t c = static_cast<uint16_t>(*chars++);
    running_hash = AddCharacterCore(running_hash, c);
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
  if (c < '0' || c > '9') {
    is_array_index_ = false;
    return false;
  }
  int d = c - '0';
  if (is_first_char_) {
    is_first_char_ = false;
    if (c == '0' && length_ > 1) {
      is_array_index_ = false;
      return false;
    }
  }
  if (array_index_ > 429496729U - ((d + 3) >> 3)) {
    is_array_index_ = false;
    return false;
  }
  array_index_ = array_index_ * 10 + d;
  return true;
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
  for (; i < length; i++) {
    DCHECK(!is_array_index_);
    AddCharacter(chars[i]);
  }
}

template <typename schar>
uint32_t StringHasher::HashSequentialString(const schar* chars, int length,
                                            uint32_t seed) {
  StringHasher hasher(length, seed);
  if (!hasher.has_trivial_hash()) hasher.AddCharacters(chars, length);
  return hasher.GetHashField();
}

IteratingStringHasher::IteratingStringHasher(int len, uint32_t seed)
    : StringHasher(len, seed) {}

uint32_t IteratingStringHasher::Hash(String* string, uint32_t seed) {
  IteratingStringHasher hasher(string->length(), seed);
  // Nothing to do.
  if (hasher.has_trivial_hash()) return hasher.GetHashField();
  ConsString* cons_string = String::VisitFlat(&hasher, string);
  if (cons_string == nullptr) return hasher.GetHashField();
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

}  // namespace internal
}  // namespace v8

#endif  // V8_STRING_HASHER_INL_H_
