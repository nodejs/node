// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_SPARSE_BIT_VECTOR_H_
#define V8_UTILS_SPARSE_BIT_VECTOR_H_

#include "src/base/bits.h"
#include "src/base/iterator.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// A sparse bit vector implementation optimized for small sizes.
// For up to {kNumBitsPerSegment} bits, no additional storage is needed, and
// accesses should be nearly as fast as {BitVector}.
class SparseBitVector : public ZoneObject {
  // 6 words for the bits plus {offset} plus {next} will be 8 machine words per
  // {Segment}. Most bit vectors are expected to fit in that single {Segment}.
  static constexpr int kNumWordsPerSegment = 6;
  static constexpr int kBitsPerWord = kBitsPerByte * kSystemPointerSize;
  static constexpr int kNumBitsPerSegment = kBitsPerWord * kNumWordsPerSegment;

  struct Segment {
    // Offset of the first bit in this segment.
    int offset = 0;
    // {words} covers bits [{offset}, {offset + kNumBitsPerSegment}).
    uintptr_t words[kNumWordsPerSegment] = {0};
    // The next segment (with strict larger offset), or {nullptr}.
    Segment* next = nullptr;

    bool empty() const {
      return std::all_of(std::begin(words), std::end(words),
                         [](auto segment) { return segment == 0; });
    }
  };

  // Check that {Segment}s are nicely aligned, for (hopefully) better cache line
  // alignment.
  static_assert(sizeof(Segment) == 8 * kSystemPointerSize);

 public:
  // An iterator to iterate all set bits.
  class Iterator : public base::iterator<std::forward_iterator_tag, int> {
   public:
    struct EndTag {};

    explicit Iterator(EndTag) {}
    explicit Iterator(const Segment* segment) {
      // {segment} is {&first_segment_}, so cannot be null initially.
      do {
        for (int word = 0; word < kNumWordsPerSegment; ++word) {
          if (segment->words[word] == 0) continue;
          int bit_in_word =
              base::bits::CountTrailingZeros(segment->words[word]);
          segment_ = segment;
          bit_in_segment_ = word * kBitsPerWord + bit_in_word;
          return;
        }
        segment = segment->next;
      } while (segment != nullptr);
      DCHECK_IMPLIES(!segment_, *this == Iterator{EndTag{}});
      DCHECK_IMPLIES(segment_,
                     contains(segment_, segment_->offset + bit_in_segment_));
    }

    int operator*() const {
      DCHECK_NOT_NULL(segment_);
      int offset = segment_->offset + bit_in_segment_;
      DCHECK(contains(segment_, offset));
      return offset;
    }

    bool operator==(const Iterator& other) const {
      return segment_ == other.segment_ &&
             bit_in_segment_ == other.bit_in_segment_;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

    void operator++() {
      int word = bit_in_segment_ / kBitsPerWord;
      int bit_in_word = bit_in_segment_ % kBitsPerWord;
      if (bit_in_word < kBitsPerWord - 1) {
        uintptr_t remaining_bits =
            segment_->words[word] &
            (std::numeric_limits<uintptr_t>::max() << (1 + bit_in_word));
        if (remaining_bits) {
          int next_bit_in_word = base::bits::CountTrailingZeros(remaining_bits);
          DCHECK_LT(bit_in_word, next_bit_in_word);
          bit_in_segment_ = word * kBitsPerWord + next_bit_in_word;
          return;
        }
      }
      // No remaining bits in the current word. Search in succeeding words and
      // segments.
      ++word;
      do {
        for (; word < kNumWordsPerSegment; ++word) {
          if (segment_->words[word] == 0) continue;
          bit_in_word = base::bits::CountTrailingZeros(segment_->words[word]);
          bit_in_segment_ = word * kBitsPerWord + bit_in_word;
          return;
        }
        segment_ = segment_->next;
        word = 0;
      } while (segment_);
      // If we reach here, there are no more bits.
      bit_in_segment_ = 0;
      DCHECK_EQ(*this, Iterator{EndTag{}});
    }

   private:
    const Segment* segment_ = nullptr;
    int bit_in_segment_ = 0;
  };

  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(SparseBitVector);

  explicit SparseBitVector(Zone* zone) : zone_(zone) {}

  V8_INLINE bool Contains(int i) const {
    DCHECK_LE(0, i);
    const Segment* segment = &first_segment_;
    // Explicit fast path for the first segment which always starts at offset 0.
    if (V8_UNLIKELY(i >= kNumBitsPerSegment)) {
      do {
        segment = segment->next;
        if (!segment) return false;
      } while (segment->offset <= i - kNumBitsPerSegment);
      if (segment->offset > i) return false;
    }
    return contains(segment, i);
  }

  V8_INLINE void Add(int i) {
    DCHECK_LE(0, i);
    Segment* last = nullptr;
    Segment* segment = &first_segment_;
    // Explicit fast path for the first segment which always starts at offset 0.
    if (V8_UNLIKELY(i >= kNumBitsPerSegment)) {
      do {
        last = segment;
        segment = segment->next;
        if (V8_UNLIKELY(!segment)) return InsertBitAfter(last, i);
      } while (segment->offset <= i - kNumBitsPerSegment);
      if (V8_UNLIKELY(segment->offset > i)) return InsertBitAfter(last, i);
    }
    set(segment, i);
  }

  V8_INLINE void Remove(int i) {
    DCHECK_LE(0, i);
    Segment* segment = &first_segment_;
    // Explicit fast path for the first segment which always starts at offset 0.
    if (V8_UNLIKELY(i >= kNumBitsPerSegment)) {
      do {
        segment = segment->next;
        if (V8_UNLIKELY(!segment)) return;
      } while (segment->offset <= i - kNumBitsPerSegment);
      if (V8_UNLIKELY(segment->offset > i)) return;
    }
    unset(segment, i);
  }

  void Union(const SparseBitVector& other) {
    // Always remember the segment before {segment}, because we sometimes need
    // to insert *before* {segment}, but we have to backlinks.
    Segment* last = nullptr;
    Segment* segment = &first_segment_;
    // Iterate all segments in {other}, merging with with existing segments in
    // {this}, or inserting new segments when needed.
    for (const Segment* other_segment = &other.first_segment_; other_segment;
         other_segment = other_segment->next) {
      // Find the first segment whose offset is >= {other_segment}'s offset.
      while (segment && segment->offset < other_segment->offset) {
        last = segment;
        segment = segment->next;
      }
      // Now either merge {other_segment} into {segment}, or insert between
      // {last} and {segment}.
      if (segment && segment->offset == other_segment->offset) {
        std::transform(std::begin(segment->words), std::end(segment->words),
                       std::begin(other_segment->words),
                       std::begin(segment->words), std::bit_or<uintptr_t>{});
      } else if (!other_segment->empty()) {
        DCHECK_LT(last->offset, other_segment->offset);
        Segment* new_segment = zone_->New<Segment>();
        new_segment->offset = other_segment->offset;
        std::copy(std::begin(other_segment->words),
                  std::end(other_segment->words),
                  std::begin(new_segment->words));
        InsertSegmentAfter(last, new_segment);
        last = new_segment;
      }
    }
  }

  Iterator begin() const { return Iterator{&first_segment_}; }
  Iterator end() const { return Iterator{Iterator::EndTag{}}; }

 private:
  static std::pair<int, int> GetWordAndBitInWord(const Segment* segment,
                                                 int i) {
    DCHECK_LE(segment->offset, i);
    DCHECK_GT(segment->offset + kNumBitsPerSegment, i);
    int bit_in_segment = i - segment->offset;
    return {bit_in_segment / kBitsPerWord, bit_in_segment % kBitsPerWord};
  }

  V8_NOINLINE void InsertBitAfter(Segment* segment, int i) {
    Segment* new_segment = zone_->New<Segment>();
    new_segment->offset = i / kNumBitsPerSegment * kNumBitsPerSegment;
    set(new_segment, i);
    InsertSegmentAfter(segment, new_segment);
    DCHECK(Contains(i));
  }

  V8_NOINLINE void InsertSegmentAfter(Segment* segment, Segment* new_segment) {
    DCHECK_NOT_NULL(segment);
    DCHECK_LT(segment->offset, new_segment->offset);
    insert_after(segment, new_segment);
    DCHECK(CheckConsistency(segment));
    DCHECK(CheckConsistency(new_segment));
  }

  static bool contains(const Segment* segment, int i) {
    auto [word, bit] = GetWordAndBitInWord(segment, i);
    return (segment->words[word] >> bit) & 1;
  }

  static bool set(Segment* segment, int i) {
    auto [word, bit] = GetWordAndBitInWord(segment, i);
    return segment->words[word] |= uintptr_t{1} << bit;
  }

  static bool unset(Segment* segment, int i) {
    auto [word, bit] = GetWordAndBitInWord(segment, i);
    return segment->words[word] &= ~(uintptr_t{1} << bit);
  }

  static void insert_after(Segment* segment, Segment* new_next) {
    DCHECK_NULL(new_next->next);
    DCHECK_LT(segment->offset, new_next->offset);
    new_next->next = segment->next;
    segment->next = new_next;
  }

  V8_WARN_UNUSED_RESULT bool CheckConsistency(const Segment* segment) {
    if ((segment->offset % kNumBitsPerSegment) != 0) return false;
    if (segment->next && segment->next->offset <= segment->offset) return false;
    return true;
  }

  Segment first_segment_;
  Zone* zone_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_SPARSE_BIT_VECTOR_H_
