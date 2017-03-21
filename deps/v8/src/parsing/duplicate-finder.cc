// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/duplicate-finder.h"

namespace v8 {
namespace internal {

bool DuplicateFinder::AddOneByteSymbol(Vector<const uint8_t> key) {
  return AddSymbol(key, true);
}

bool DuplicateFinder::AddTwoByteSymbol(Vector<const uint16_t> key) {
  return AddSymbol(Vector<const uint8_t>::cast(key), false);
}

bool DuplicateFinder::AddSymbol(Vector<const uint8_t> key, bool is_one_byte) {
  uint32_t hash = Hash(key, is_one_byte);
  byte* encoding = BackupKey(key, is_one_byte);
  base::HashMap::Entry* entry = map_.LookupOrInsert(encoding, hash);
  int old_value = static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
  entry->value = reinterpret_cast<void*>(1);
  return old_value;
}

uint32_t DuplicateFinder::Hash(Vector<const uint8_t> key, bool is_one_byte) {
  // Primitive hash function, almost identical to the one used
  // for strings (except that it's seeded by the length and representation).
  int length = key.length();
  uint32_t hash = (length << 1) | (is_one_byte ? 1 : 0);
  for (int i = 0; i < length; i++) {
    uint32_t c = key[i];
    hash = (hash + c) * 1025;
    hash ^= (hash >> 6);
  }
  return hash;
}

bool DuplicateFinder::Match(void* first, void* second) {
  // Decode lengths.
  // Length + representation is encoded as base 128, most significant heptet
  // first, with a 8th bit being non-zero while there are more heptets.
  // The value encodes the number of bytes following, and whether the original
  // was Latin1.
  byte* s1 = reinterpret_cast<byte*>(first);
  byte* s2 = reinterpret_cast<byte*>(second);
  uint32_t length_one_byte_field = 0;
  byte c1;
  do {
    c1 = *s1;
    if (c1 != *s2) return false;
    length_one_byte_field = (length_one_byte_field << 7) | (c1 & 0x7f);
    s1++;
    s2++;
  } while ((c1 & 0x80) != 0);
  int length = static_cast<int>(length_one_byte_field >> 1);
  return memcmp(s1, s2, length) == 0;
}

byte* DuplicateFinder::BackupKey(Vector<const uint8_t> bytes,
                                 bool is_one_byte) {
  uint32_t one_byte_length = (bytes.length() << 1) | (is_one_byte ? 1 : 0);
  backing_store_.StartSequence();
  // Emit one_byte_length as base-128 encoded number, with the 7th bit set
  // on the byte of every heptet except the last, least significant, one.
  if (one_byte_length >= (1 << 7)) {
    if (one_byte_length >= (1 << 14)) {
      if (one_byte_length >= (1 << 21)) {
        if (one_byte_length >= (1 << 28)) {
          backing_store_.Add(
              static_cast<uint8_t>((one_byte_length >> 28) | 0x80));
        }
        backing_store_.Add(
            static_cast<uint8_t>((one_byte_length >> 21) | 0x80u));
      }
      backing_store_.Add(static_cast<uint8_t>((one_byte_length >> 14) | 0x80u));
    }
    backing_store_.Add(static_cast<uint8_t>((one_byte_length >> 7) | 0x80u));
  }
  backing_store_.Add(static_cast<uint8_t>(one_byte_length & 0x7f));

  backing_store_.AddBlock(bytes);
  return backing_store_.EndSequence().start();
}

}  // namespace internal
}  // namespace v8
