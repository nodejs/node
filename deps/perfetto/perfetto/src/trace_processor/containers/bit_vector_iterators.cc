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

#include "src/trace_processor/containers/bit_vector_iterators.h"

namespace perfetto {
namespace trace_processor {
namespace internal {

BaseIterator::BaseIterator(BitVector* bv) : bv_(bv) {
  size_ = bv->size();

  if (size_ > 0) {
    block_ = bv_->blocks_[0];
  }
}

BaseIterator::~BaseIterator() {
  if (size_ > 0) {
    uint32_t block_idx = index_ / BitVector::Block::kBits;
    uint32_t last_block_idx = static_cast<uint32_t>(bv_->blocks_.size()) - 1;

    // If |index_| == |size_| and the last index was on a block boundary, we
    // can end up one block past the end of the bitvector. Take the
    // min of the block index and the last block
    OnBlockChange(std::min(block_idx, last_block_idx), last_block_idx);
  }
}

void BaseIterator::OnBlockChange(uint32_t old_block, uint32_t new_block) {
  // If we touched the current block, flush the block to the bitvector.
  if (is_block_changed_) {
    bv_->blocks_[old_block] = block_;
  }

  if (set_bit_count_diff_ != 0) {
    // If the count of set bits has changed, go through all the counts between
    // the old and new blocks and modify them.
    // We only need to go to new_block and not to the end of the bitvector as
    // the blocks after new_block will either be updated in a future call to
    // OnBlockChange or in the destructor.
    for (uint32_t i = old_block + 1; i <= new_block; ++i) {
      int32_t new_count =
          static_cast<int32_t>(bv_->counts_[i]) + set_bit_count_diff_;
      PERFETTO_DCHECK(new_count >= 0);

      bv_->counts_[i] = static_cast<uint32_t>(new_count);
    }
  }

  // Reset the changed flag and cache the new block.
  is_block_changed_ = false;
  block_ = bv_->blocks_[new_block];
}

AllBitsIterator::AllBitsIterator(const BitVector* bv)
    : BaseIterator(const_cast<BitVector*>(bv)) {}

SetBitsIterator::SetBitsIterator(const BitVector* bv)
    : BaseIterator(const_cast<BitVector*>(bv)) {
  set_bit_count_ = bv->GetNumBitsSet();

  if (set_bit_count_ > 0) {
    // Read a batch of set bit indices starting at index 0.
    ReadSetBitBatch(0);

    // Fast forward the iterator to the first index in the freshly read
    // batch of set bots.
    SetIndex(batch_[0]);
  }
}

void SetBitsIterator::ReadSetBitBatch(uint32_t start_idx) {
  PERFETTO_DCHECK(set_bit_index_ % kBatchSize == 0);

  uint32_t set_bit_count_until_i = set_bit_index_;
  for (uint32_t i = start_idx; i < size(); ++i) {
    auto addr = BitVector::IndexToAddress(i);

    // Compute the count to the end of the block noting that the last block
    // needs to use |set_bit_count_| and not the next count in the vector
    // because that is OOB.
    uint32_t set_bits_to_end_of_block =
        addr.block_idx == bv().counts_.size() - 1
            ? set_bit_count_
            : bv().counts_[addr.block_idx + 1];

    // Optimization: If the count of set bits to the end of the block is the
    // same as the count to the current index, we can just skip the whole
    // block without iterating through the bits inside.
    if (set_bits_to_end_of_block == set_bit_count_until_i) {
      static constexpr BitVector::BlockOffset kLastBlockOffset = {
          BitVector::Block::kWords - 1, BitVector::BitWord::kBits - 1};

      i = BitVector::AddressToIndex({addr.block_idx, kLastBlockOffset});
      continue;
    }

    // If the bit is not set, just bail out.
    const auto& block = bv().blocks_[addr.block_idx];
    if (!block.IsSet(addr.block_offset))
      continue;

    // Update |batch_| with the index of the current bit.
    uint32_t batch_idx = set_bit_count_until_i++ % kBatchSize;
    batch_[batch_idx] = i;

    // If we've reached as many indicies as the batch can store, just
    // return.
    if (PERFETTO_UNLIKELY(batch_idx == kBatchSize - 1))
      return;
  }

  // We should only get here when we've managed to read all the set bits.
  // End of batch should return from the body of the loop.
  PERFETTO_DCHECK(set_bit_count_until_i == set_bit_count_);
}

}  // namespace internal
}  // namespace trace_processor
}  // namespace perfetto
