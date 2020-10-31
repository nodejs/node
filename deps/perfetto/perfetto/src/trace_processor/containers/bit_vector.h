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

#ifndef SRC_TRACE_PROCESSOR_CONTAINERS_BIT_VECTOR_H_
#define SRC_TRACE_PROCESSOR_CONTAINERS_BIT_VECTOR_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <array>
#include <vector>

#include "perfetto/base/logging.h"

namespace perfetto {
namespace trace_processor {

namespace internal {

class BaseIterator;
class AllBitsIterator;
class SetBitsIterator;

}  // namespace internal

// A bitvector which compactly stores a vector of bools using a single bit
// for each bool.
class BitVector {
 public:
  using AllBitsIterator = internal::AllBitsIterator;
  using SetBitsIterator = internal::SetBitsIterator;

  // Creates an empty bitvector.
  BitVector();

  explicit BitVector(std::initializer_list<bool> init);

  // Creates a bitvector of |count| size filled with |value|.
  BitVector(uint32_t count, bool value = false);

  // Enable moving bitvectors as they have no unmovable state.
  BitVector(BitVector&&) noexcept = default;
  BitVector& operator=(BitVector&&) = default;

  // Create a copy of the bitvector.
  BitVector Copy() const;

  // Returns the size of the bitvector.
  uint32_t size() const { return static_cast<uint32_t>(size_); }

  // Returns whether the bit at |idx| is set.
  bool IsSet(uint32_t idx) const {
    PERFETTO_DCHECK(idx < size());

    Address a = IndexToAddress(idx);
    return blocks_[a.block_idx].IsSet(a.block_offset);
  }

  // Returns the number of set bits in the bitvector.
  uint32_t GetNumBitsSet() const { return GetNumBitsSet(size()); }

  // Returns the number of set bits between the start of the bitvector
  // (inclusive) and the index |end| (exclusive).
  uint32_t GetNumBitsSet(uint32_t end) const {
    if (end == 0)
      return 0;

    // Although the external interface we present uses an exclusive |end|,
    // internally it's a lot nicer to work with an inclusive |end| (mainly
    // because we get block rollovers on exclusive ends which means we need
    // to have if checks to ensure we don't overflow the number of blocks).
    Address addr = IndexToAddress(end - 1);
    uint32_t idx = addr.block_idx;

    // Add the number of set bits until the start of the block to the number
    // of set bits until the end address inside the block.
    return counts_[idx] + blocks_[idx].GetNumBitsSet(addr.block_offset);
  }

  // Returns the index of the |n|th set bit. Should only be called with |n| <
  // GetNumBitsSet().
  uint32_t IndexOfNthSet(uint32_t n) const {
    PERFETTO_DCHECK(n < GetNumBitsSet());

    // First search for the block which, up until the start of it, has more than
    // n bits set. Note that this should never return |counts.begin()| as
    // that should always be 0.
    // TODO(lalitm): investigate whether we can make this faster with small
    // binary search followed by a linear search instead of binary searching the
    // full way.
    auto it = std::upper_bound(counts_.begin(), counts_.end(), n);
    PERFETTO_DCHECK(it != counts_.begin());

    // Go back one block to find the block which has the bit we are looking for.
    uint32_t block_idx =
        static_cast<uint32_t>(std::distance(counts_.begin(), it) - 1);

    // Figure out how many set bits forward we are looking inside the block
    // by taking away the number of bits at the start of the block from n.
    uint32_t set_in_block = n - counts_[block_idx];

    // Compute the address of the bit in the block then convert the full
    // address back to an index.
    BlockOffset block_offset = blocks_[block_idx].IndexOfNthSet(set_in_block);
    return AddressToIndex(Address{block_idx, block_offset});
  }

  // Sets the bit at index |idx| to true.
  void Set(uint32_t idx) {
    // Set the bit to the correct value inside the block but store the old
    // bit to help fix the counts.
    auto addr = IndexToAddress(idx);
    bool old_value = blocks_[addr.block_idx].IsSet(addr.block_offset);

    // If the old value was unset, set the bit and add one to the count.
    if (PERFETTO_LIKELY(!old_value)) {
      blocks_[addr.block_idx].Set(addr.block_offset);

      uint32_t size = static_cast<uint32_t>(counts_.size());
      for (uint32_t i = addr.block_idx + 1; i < size; ++i) {
        counts_[i]++;
      }
    }
  }

  // Sets the bit at index |idx| to false.
  void Clear(uint32_t idx) {
    // Set the bit to the correct value inside the block but store the old
    // bit to help fix the counts.
    auto addr = IndexToAddress(idx);
    bool old_value = blocks_[addr.block_idx].IsSet(addr.block_offset);

    // If the old value was set, clear the bit and subtract one from all the
    // counts.
    if (PERFETTO_LIKELY(old_value)) {
      blocks_[addr.block_idx].Clear(addr.block_offset);

      uint32_t size = static_cast<uint32_t>(counts_.size());
      for (uint32_t i = addr.block_idx + 1; i < size; ++i) {
        counts_[i]--;
      }
    }
  }

  // Appends true to the bitvector.
  void AppendTrue() {
    Address addr = IndexToAddress(size_);
    uint32_t old_blocks_size = static_cast<uint32_t>(blocks_.size());
    uint32_t new_blocks_size = addr.block_idx + 1;

    if (PERFETTO_UNLIKELY(new_blocks_size > old_blocks_size)) {
      uint32_t t = GetNumBitsSet();
      blocks_.emplace_back();
      counts_.emplace_back(t);
    }

    size_++;
    blocks_[addr.block_idx].Set(addr.block_offset);
  }

  // Appends false to the bitvector.
  void AppendFalse() {
    Address addr = IndexToAddress(size_);
    uint32_t old_blocks_size = static_cast<uint32_t>(blocks_.size());
    uint32_t new_blocks_size = addr.block_idx + 1;

    if (PERFETTO_UNLIKELY(new_blocks_size > old_blocks_size)) {
      uint32_t t = GetNumBitsSet();
      blocks_.emplace_back();
      counts_.emplace_back(t);
    }

    size_++;
    // We don't need to clear the bit as we ensure that anything after
    // size_ is always set to false.
  }

  // Resizes the BitVector to the given |size|.
  // Truncates the BitVector if |size| < |size()| or fills the new space with
  // |value| if |size| > |size()|. Calling this method is a noop if |size| ==
  // |size()|.
  void Resize(uint32_t size, bool value = false) {
    uint32_t old_size = size_;
    if (size == old_size)
      return;

    // Empty bitvectors should be memory efficient so we don't keep any data
    // around in the bitvector.
    if (size == 0) {
      blocks_.clear();
      counts_.clear();
      size_ = 0;
      return;
    }

    // Compute the address of the new last bit in the bitvector.
    Address last_addr = IndexToAddress(size - 1);
    uint32_t old_blocks_size = static_cast<uint32_t>(counts_.size());
    uint32_t new_blocks_size = last_addr.block_idx + 1;

    // Then, resize the block and count vectors to have the correct
    // number of entries.
    blocks_.resize(new_blocks_size);
    counts_.resize(new_blocks_size);

    if (size > old_size) {
      if (value) {
        // If the new space should be filled with true, then set all the bits
        // between the address of the old size and the new last address.
        const Address& start = IndexToAddress(old_size);
        Set(start, last_addr);

        // We then need to update the counts vector to match the changes we
        // made to the blocks.

        // We start by adding the bits we set in the first block to the
        // cummulative count before the range we changed.
        Address end_of_block = {start.block_idx,
                                {Block::kWords - 1, BitWord::kBits - 1}};
        uint32_t count_in_block_after_end =
            AddressToIndex(end_of_block) - AddressToIndex(start) + 1;
        uint32_t set_count = GetNumBitsSet() + count_in_block_after_end;

        for (uint32_t i = start.block_idx + 1; i <= last_addr.block_idx; ++i) {
          // Set the count to the cummulative count so far.
          counts_[i] = set_count;

          // Add a full block of set bits to the count.
          set_count += Block::kBits;
        }
      } else {
        // If the newly added bits are false, we just need to update the
        // counts vector with the current size of the bitvector for all
        // the newly added blocks.
        if (new_blocks_size > old_blocks_size) {
          uint32_t count = GetNumBitsSet();
          for (uint32_t i = old_blocks_size; i < new_blocks_size; ++i) {
            counts_[i] = count;
          }
        }
      }
    } else {
      // Throw away all the bits after the new last bit. We do this to make
      // future lookup, append and resize operations not have to worrying about
      // trailing garbage bits in the last block.
      blocks_[last_addr.block_idx].ClearAfter(last_addr.block_offset);
    }

    // Actually update the size.
    size_ = size;
  }

  // Creates a BitVector of size |end| with the bits between |start| and |end|
  // filled by calling the filler function |f(index of bit)|.
  //
  // As an example, suppose Range(3, 7, [](x) { return x < 5 }). This would
  // result in the following bitvector:
  // [0 0 0 1 1 0 0 0]
  template <typename Filler = bool(uint32_t)>
  static BitVector Range(uint32_t start, uint32_t end, Filler f) {
    // Compute the block index and bitvector index where we start and end
    // working one block at a time.
    uint32_t start_fast_block = BlockCeil(start);
    uint32_t start_fast_idx = BlockToIndex(start_fast_block);
    uint32_t end_fast_block = BlockFloor(end);
    uint32_t end_fast_idx = BlockToIndex(end_fast_block);

    // First, create the BitVector up to |start| then fill up to
    // |start_fast_index| with values from the filler.
    BitVector bv(start, false);
    for (uint32_t i = start; i < start_fast_idx; ++i) {
      bv.Append(f(i));
    }

    // At this point we can work one block at a time.
    for (uint32_t i = start_fast_block; i < end_fast_block; ++i) {
      bv.counts_.emplace_back(bv.GetNumBitsSet());
      bv.blocks_.emplace_back(Block::FromFiller(bv.size_, f));
      bv.size_ += Block::kBits;
    }

    // Add the last few elements to finish up to |end|.
    for (uint32_t i = end_fast_idx; i < end; ++i) {
      bv.Append(f(i));
    }
    return bv;
  }

  // Updates the ith set bit of this bitvector with the value of
  // |other.IsSet(i)|.
  //
  // This is the best way to batch update all the bits which are set; for
  // example when filtering rows, we want to filter all rows which are currently
  // included but ignore rows which have already been excluded.
  //
  // For example suppose the following:
  // this:  1 1 0 0 1 0 1
  // other: 0 1 1 0
  // This will change this to the following:
  // this:  0 1 0 0 1 0 0
  // TODO(lalitm): investigate whether we should just change this to And.
  void UpdateSetBits(const BitVector& other);

  // Iterate all the bits in the BitVector.
  //
  // Usage:
  // for (auto it = bv.IterateAllBits(); it; it.Next()) {
  //   ...
  // }
  AllBitsIterator IterateAllBits() const;

  // Iterate all the set bits in the BitVector.
  //
  // Usage:
  // for (auto it = bv.IterateSetBits(); it; it.Next()) {
  //   ...
  // }
  SetBitsIterator IterateSetBits() const;

  // Returns the approximate cost (in bytes) of storing a bitvector with size
  // |n|. This can be used to make decisions about whether using a BitVector is
  // worthwhile.
  // This cost should not be treated as exact - it just gives an indication of
  // the memory needed.
  static constexpr uint32_t ApproxBytesCost(uint32_t n) {
    // The two main things making up a bitvector is the cost of the blocks of
    // bits and the cost of the counts vector.
    return BlockCeil(n) * Block::kBits + BlockCeil(n) * sizeof(uint32_t);
  }

 private:
  friend class internal::BaseIterator;
  friend class internal::AllBitsIterator;
  friend class internal::SetBitsIterator;

  // Represents the offset of a bit within a block.
  struct BlockOffset {
    uint16_t word_idx;
    uint16_t bit_idx;
  };

  // Represents the address of a bit within the bitvector.
  struct Address {
    uint32_t block_idx;
    BlockOffset block_offset;
  };

  // Represents the smallest collection of bits we can refer to as
  // one unit.
  //
  // Currently, this is implemented as a 64 bit integer as this is the
  // largest type which we can assume to be present on all platforms.
  class BitWord {
   public:
    static constexpr uint32_t kBits = 64;

    // Returns whether the bit at the given index is set.
    bool IsSet(uint32_t idx) const {
      PERFETTO_DCHECK(idx < kBits);
      return (word_ >> idx) & 1ull;
    }

    // Bitwise ors the given |mask| to the current value.
    void Or(uint64_t mask) { word_ |= mask; }

    // Sets the bit at the given index to true.
    void Set(uint32_t idx) {
      PERFETTO_DCHECK(idx < kBits);

      // Or the value for the true shifted up to |idx| with the word.
      Or(1ull << idx);
    }

    // Sets the bit at the given index to false.
    void Clear(uint32_t idx) {
      PERFETTO_DCHECK(idx < kBits);

      // And the integer of all bits set apart from |idx| with the word.
      word_ &= ~(1ull << idx);
    }

    // Clears all the bits (i.e. sets the atom to zero).
    void ClearAll() { word_ = 0; }

    // Returns the index of the nth set bit.
    // Undefined if |n| >= |GetNumBitsSet()|.
    uint16_t IndexOfNthSet(uint32_t n) const {
      PERFETTO_DCHECK(n < kBits);

      // The below code is very dense but essentially computes the nth set
      // bit inside |atom| in the "broadword" style of programming (sometimes
      // referred to as "SIMD within a register").
      //
      // Instead of treating a uint64 as an individual unit, broadword
      // algorithms treat them as a packed vector of uint8. By doing this, they
      // allow branchless algorithms when considering bits of a uint64.
      //
      // In benchmarks, this algorithm has found to be the fastest, portable
      // way of computing the nth set bit (if we were only targetting new
      // versions of x64, we could also use pdep + ctz but unfortunately
      // this would fail on WASM - this about 2.5-3x faster on x64).
      //
      // The code below was taken from the paper
      // http://vigna.di.unimi.it/ftp/papers/Broadword.pdf
      uint64_t s = word_ - ((word_ & 0xAAAAAAAAAAAAAAAA) >> 1);
      s = (s & 0x3333333333333333) + ((s >> 2) & 0x3333333333333333);
      s = ((s + (s >> 4)) & 0x0F0F0F0F0F0F0F0F) * L8;

      uint64_t b = (BwLessThan(s, n * L8) >> 7) * L8 >> 53 & ~7ull;
      uint64_t l = n - ((s << 8) >> b & 0xFF);
      s = (BwGtZero(((word_ >> b & 0xFF) * L8) & 0x8040201008040201) >> 7) * L8;

      uint64_t ret = b + ((BwLessThan(s, l * L8) >> 7) * L8 >> 56);

      return static_cast<uint16_t>(ret);
    }

    // Returns the number of set bits.
    uint32_t GetNumBitsSet() const {
      // We use __builtin_popcountll here as it's available natively for the two
      // targets we care most about (x64 and WASM).
      return static_cast<uint32_t>(__builtin_popcountll(word_));
    }

    // Returns the number of set bits up to and including the bit at |idx|.
    uint32_t GetNumBitsSet(uint32_t idx) const {
      PERFETTO_DCHECK(idx < kBits);

      // We use __builtin_popcountll here as it's available natively for the two
      // targets we care most about (x64 and WASM).
      return static_cast<uint32_t>(__builtin_popcountll(WordUntil(idx)));
    }

    // Retains all bits up to and including the bit at |idx| and clears
    // all bits after this point.
    void ClearAfter(uint32_t idx) {
      PERFETTO_DCHECK(idx < kBits);
      word_ = WordUntil(idx);
    }

    // Sets all bits between the bit at |start| and |end| (inclusive).
    void Set(uint32_t start, uint32_t end) {
      uint32_t diff = end - start;
      word_ |= (MaskAllBitsSetUntil(diff) << static_cast<uint64_t>(start));
    }

   private:
    // Constant with all the low bit of every byte set.
    static constexpr uint64_t L8 = 0x0101010101010101;

    // Constant with all the high bit of every byte set.
    static constexpr uint64_t H8 = 0x8080808080808080;

    // Returns a packed uint64 encoding whether each byte of x is less
    // than each corresponding byte of y.
    // This is computed in the "broadword" style of programming; see
    // IndexOfNthSet for details on this.
    static uint64_t BwLessThan(uint64_t x, uint64_t y) {
      return (((y | H8) - (x & ~H8)) ^ x ^ y) & H8;
    }

    // Returns a packed uint64 encoding whether each byte of x is greater
    // than or equal zero.
    // This is computed in the "broadword" style of programming; see
    // IndexOfNthSet for details on this.
    static uint64_t BwGtZero(uint64_t x) { return (((x | H8) - L8) | x) & H8; }

    // Returns the bits up to and including the bit at |idx|.
    uint64_t WordUntil(uint32_t idx) const {
      PERFETTO_DCHECK(idx < kBits);

      // To understand what is happeninng here, consider an example.
      // Suppose we want to all the bits up to the 7th bit in the atom
      //               7th
      //                |
      //                v
      // atom: 01010101011111000
      //
      // The easiest way to do this would be if we had a mask with only
      // the bottom 7 bits set:
      // mask: 00000000001111111
      uint64_t mask = MaskAllBitsSetUntil(idx);

      // Finish up by anding the the atom with the computed msk.
      return word_ & mask;
    }

    // Return a mask of all the bits up to and including bit at |idx|.
    static uint64_t MaskAllBitsSetUntil(uint32_t idx) {
      // Start with 1 and shift it up (idx + 1) bits we get:
      // top : 00000000010000000
      uint64_t top = 1ull << ((idx + 1ull) % kBits);

      // We need to handle the case where idx == 63. In this case |top| will be
      // zero because 1 << ((idx + 1) % 64) == 1 << (64 % 64) == 1.
      // In this case, we actually want top == 0. We can do this by shifting
      // down by (idx + 1) / kBits - this will be a noop for every index other
      // than idx == 63. This should also be free on x86 because of the mod
      // instruction above.
      top = top >> ((idx + 1) / kBits);

      // Then if we take away 1, we get precisely the mask we want.
      return top - 1u;
    }

    uint64_t word_ = 0;
  };

  // Represents a group of bits with a bitcount such that it is
  // efficient to work on these bits.
  //
  // On x86 architectures we generally target for trace processor, the
  // size of a cache line is 64 bytes (or 512 bits). For this reason,
  // we make the size of the block contain 8 atoms as 8 * 64 == 512.
  //
  // TODO(lalitm): investigate whether we should tune this value for
  // WASM and ARM.
  class Block {
   public:
    // See class documentation for how these constants are chosen.
    static constexpr uint16_t kWords = 8;
    static constexpr uint32_t kBits = kWords * BitWord::kBits;

    // Returns whether the bit at the given address is set.
    bool IsSet(const BlockOffset& addr) const {
      PERFETTO_DCHECK(addr.word_idx < kWords);

      return words_[addr.word_idx].IsSet(addr.bit_idx);
    }

    // Sets the bit at the given address to true.
    void Set(const BlockOffset& addr) {
      PERFETTO_DCHECK(addr.word_idx < kWords);

      words_[addr.word_idx].Set(addr.bit_idx);
    }

    // Sets the bit at the given address to false.
    void Clear(const BlockOffset& addr) {
      PERFETTO_DCHECK(addr.word_idx < kWords);

      words_[addr.word_idx].Clear(addr.bit_idx);
    }

    // Gets the offset of the nth set bit in this block.
    BlockOffset IndexOfNthSet(uint32_t n) const {
      uint32_t count = 0;
      for (uint16_t i = 0; i < kWords; ++i) {
        // Keep a running count of all the set bits in the atom.
        uint32_t value = count + words_[i].GetNumBitsSet();
        if (value <= n) {
          count = value;
          continue;
        }

        // The running count of set bits is more than |n|. That means this atom
        // contains the bit we are looking for.

        // Take away the number of set bits to the start of this atom from |n|.
        uint32_t set_in_atom = n - count;

        // Figure out the index of the set bit inside the atom and create the
        // address of this bit from that.
        uint16_t bit_idx = words_[i].IndexOfNthSet(set_in_atom);
        PERFETTO_DCHECK(bit_idx < 64);
        return BlockOffset{i, bit_idx};
      }
      PERFETTO_FATAL("Index out of bounds");
    }

    // Gets the number of set bits within a block up to and including the bit
    // at the given address.
    uint32_t GetNumBitsSet(const BlockOffset& addr) const {
      PERFETTO_DCHECK(addr.word_idx < kWords);

      // Count all the set bits in the atom until we reach the last atom
      // index.
      uint32_t count = 0;
      for (uint32_t i = 0; i < addr.word_idx; ++i) {
        count += words_[i].GetNumBitsSet();
      }

      // For the last atom, only count the bits upto and including the bit
      // index.
      return count + words_[addr.word_idx].GetNumBitsSet(addr.bit_idx);
    }

    // Retains all bits up to and including the bit at |addr| and clears
    // all bits after this point.
    void ClearAfter(const BlockOffset& offset) {
      PERFETTO_DCHECK(offset.word_idx < kWords);

      // In the first atom, keep the bits until the address specified.
      words_[offset.word_idx].ClearAfter(offset.bit_idx);

      // For all subsequent atoms, we just clear the whole atom.
      for (uint32_t i = offset.word_idx + 1; i < kWords; ++i) {
        words_[i].ClearAll();
      }
    }

    // Set all the bits between the offsets given by |start| and |end|
    // (inclusive).
    void Set(const BlockOffset& start, const BlockOffset& end) {
      if (start.word_idx == end.word_idx) {
        // If there is only one word we will change, just set the range within
        // the word.
        words_[start.word_idx].Set(start.bit_idx, end.bit_idx);
        return;
      }

      // Otherwise, we have more than one word to set. To do this, we will
      // do this in three steps.

      // First, we set the first word from the start to the end of the word.
      words_[start.word_idx].Set(start.bit_idx, BitWord::kBits - 1);

      // Next, we set all words (except the last).
      for (uint32_t i = start.word_idx + 1; i < end.word_idx; ++i) {
        words_[i].Set(0, BitWord::kBits - 1);
      }

      // Finally, we set the word block from the start to the end offset.
      words_[end.word_idx].Set(0, end.bit_idx);
    }

    template <typename Filler>
    static Block FromFiller(uint32_t offset, Filler f) {
      // We choose to iterate the bits as the outer loop as this allows us
      // to reuse the mask and the bit offset between iterations of the loop.
      // This makes a small (but noticable) impact in the performance of this
      // function.
      Block b;
      for (uint32_t i = 0; i < BitWord::kBits; ++i) {
        uint64_t mask = 1ull << i;
        uint32_t offset_with_bit = offset + i;
        for (uint32_t j = 0; j < Block::kWords; ++j) {
          bool res = f(offset_with_bit + j * BitWord::kBits);
          b.words_[j].Or(res ? mask : 0);
        }
      }
      return b;
    }

   private:
    std::array<BitWord, kWords> words_{};
  };

  BitVector(std::vector<Block> blocks,
            std::vector<uint32_t> counts,
            uint32_t size);

  BitVector(const BitVector&) = delete;
  BitVector& operator=(const BitVector&) = delete;

  // Set all the bits between the addresses given by |start| and |end|
  // (inclusive).
  // Note: this method does not update the counts vector - that is the
  // responibility of the caller.
  void Set(const Address& start, const Address& end) {
    static constexpr BlockOffset kFirstBlockOffset = BlockOffset{0, 0};
    static constexpr BlockOffset kLastBlockOffset =
        BlockOffset{Block::kWords - 1, BitWord::kBits - 1};

    if (start.block_idx == end.block_idx) {
      // If there is only one block we will change, just set the range within
      // the block.
      blocks_[start.block_idx].Set(start.block_offset, end.block_offset);
      return;
    }

    // Otherwise, we have more than one block to set. To do this, we will
    // do this in three steps.

    // First, we set the first block from the start to the end of the block.
    blocks_[start.block_idx].Set(start.block_offset, kLastBlockOffset);

    // Next, we set all blocks (except the last).
    for (uint32_t i = start.block_idx + 1; i < end.block_idx; ++i) {
      blocks_[i].Set(kFirstBlockOffset, kLastBlockOffset);
    }

    // Finally, we set the last block from the start to the end offset.
    blocks_[end.block_idx].Set(kFirstBlockOffset, end.block_offset);
  }

  // Helper function to append a bit. Generally, prefer to call AppendTrue
  // or AppendFalse instead of this function if you know the type - they will
  // be faster.
  void Append(bool value) {
    if (value) {
      AppendTrue();
    } else {
      AppendFalse();
    }
  }

  static Address IndexToAddress(uint32_t idx) {
    Address a;
    a.block_idx = idx / Block::kBits;

    uint16_t bit_idx_inside_block = idx % Block::kBits;
    a.block_offset.word_idx = bit_idx_inside_block / BitWord::kBits;
    a.block_offset.bit_idx = bit_idx_inside_block % BitWord::kBits;
    return a;
  }

  static uint32_t AddressToIndex(Address addr) {
    return addr.block_idx * Block::kBits +
           addr.block_offset.word_idx * BitWord::kBits +
           addr.block_offset.bit_idx;
  }

  // Rounds |idx| up to the nearest block boundary and returns the block
  // index. If |idx| is already on a block boundary, the current block is
  // returned.
  //
  // This is useful to be able to find indices where "fast" algorithms can start
  // which work on entire blocks.
  static constexpr uint32_t BlockCeil(uint32_t idx) {
    // Adding |Block::kBits - 1| gives us a quick way to get the ceil. We
    // do this instead of adding 1 at the end because that gives incorrect
    // answers for index % Block::kBits == 0.
    return (idx + Block::kBits - 1) / Block::kBits;
  }

  // Returns the index of the block which would store |idx|.
  static constexpr uint32_t BlockFloor(uint32_t idx) {
    return idx / Block::kBits;
  }

  // Converts a block index to a index in the BitVector.
  static constexpr uint32_t BlockToIndex(uint32_t block) {
    return block * Block::kBits;
  }

  uint32_t size_ = 0;
  std::vector<uint32_t> counts_;
  std::vector<Block> blocks_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_CONTAINERS_BIT_VECTOR_H_
