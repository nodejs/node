/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/containers/bit_vector.h"

#include "src/trace_processor/containers/bit_vector_iterators.h"

namespace perfetto {
namespace trace_processor {

BitVector::BitVector() = default;

BitVector::BitVector(std::initializer_list<bool> init) {
  for (bool x : init) {
    if (x) {
      AppendTrue();
    } else {
      AppendFalse();
    }
  }
}

BitVector::BitVector(uint32_t count, bool value) {
  Resize(count, value);
}

BitVector::BitVector(std::vector<Block> blocks,
                     std::vector<uint32_t> counts,
                     uint32_t size)
    : size_(size), counts_(std::move(counts)), blocks_(std::move(blocks)) {}

BitVector BitVector::Copy() const {
  return BitVector(blocks_, counts_, size_);
}

BitVector::AllBitsIterator BitVector::IterateAllBits() const {
  return AllBitsIterator(this);
}

BitVector::SetBitsIterator BitVector::IterateSetBits() const {
  return SetBitsIterator(this);
}

void BitVector::UpdateSetBits(const BitVector& o) {
  PERFETTO_DCHECK(o.size() <= GetNumBitsSet());

  // For each set bit in this bitvector, we lookup whether the bit in |other|
  // at that index (if in bounds) is set. If not, we clear the bit.
  for (auto it = IterateSetBits(); it; it.Next()) {
    if (it.ordinal() >= o.size() || !o.IsSet(it.ordinal()))
      it.Clear();
  }

  // After the loop, we should have precisely the same number of bits
  // set as |other|.
  PERFETTO_DCHECK(o.GetNumBitsSet() == GetNumBitsSet());
}

}  // namespace trace_processor
}  // namespace perfetto
