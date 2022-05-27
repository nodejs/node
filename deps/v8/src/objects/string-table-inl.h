// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRING_TABLE_INL_H_
#define V8_OBJECTS_STRING_TABLE_INL_H_

#include "src/objects/string-table.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

StringTableKey::StringTableKey(uint32_t raw_hash_field, int length)
    : raw_hash_field_(raw_hash_field), length_(length) {}

void StringTableKey::set_raw_hash_field(uint32_t raw_hash_field) {
  raw_hash_field_ = raw_hash_field;
}

uint32_t StringTableKey::hash() const {
  return Name::HashBits::decode(raw_hash_field_);
}

int StringForwardingTable::Size() { return next_free_index_; }

// static
uint32_t StringForwardingTable::BlockForIndex(int index,
                                              uint32_t* index_in_block) {
  DCHECK_GE(index, 0);
  DCHECK_NOT_NULL(index_in_block);
  // The block is the leftmost set bit of the index, corrected by the size
  // of the first block.
  const uint32_t block = kBitsPerInt -
                         base::bits::CountLeadingZeros(
                             static_cast<uint32_t>(index + kInitialBlockSize)) -
                         kInitialBlockSizeHighestBit - 1;
  *index_in_block = IndexInBlock(index, block);
  return block;
}

// static
uint32_t StringForwardingTable::IndexInBlock(int index, uint32_t block) {
  DCHECK_GE(index, 0);
  // Clear out the leftmost set bit (the block) to get the index within the
  // block.
  return static_cast<uint32_t>(index + kInitialBlockSize) &
         ~(1u << (block + kInitialBlockSizeHighestBit));
}

// static
uint32_t StringForwardingTable::CapacityForBlock(uint32_t block) {
  return 1u << (block + kInitialBlockSizeHighestBit);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRING_TABLE_INL_H_
