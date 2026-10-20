// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_BIT_VECTOR_H_
#define V8_UTILS_BIT_VECTOR_H_

#include <algorithm>

#include "src/base/bits.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class GrowableBitVector;

class V8_EXPORT_PRIVATE BitVector : public ZoneObject {
 private:
  class IteratorBase {
   public:
    int operator*() const {
      DCHECK_NE(end_, ptr_);
      DCHECK(target_->Contains(current_index_));
      return current_index_;
    }

    bool operator==(const IteratorBase& other) const {
      DCHECK_EQ(target_, other.target_);
      DCHECK_EQ(end_, other.end_);
      DCHECK_IMPLIES(current_index_ == other.current_index_,
                     ptr_ == other.ptr_);
      return current_index_ == other.current_index_;
    }

   protected:
    IteratorBase(const BitVector* target, uintptr_t* ptr, uintptr_t* end,
                 int current_index)
        : ptr_(ptr),
          end_(end),
#ifdef DEBUG
          target_(target),
#endif
          current_index_(current_index) {
    }

    static constexpr struct StartTag {
    } kStartTag = {};
    static constexpr struct EndTag {
    } kEndTag = {};

    uintptr_t* ptr_;
    uintptr_t* end_;
#ifdef DEBUG
    const BitVector* target_;
#endif
    int current_index_;
  };  // IteratorBase

 public:
  // Iterator for the elements of this BitVector.
  class Iterator : public IteratorBase {
   public:
    V8_EXPORT_PRIVATE inline void operator++() {
      int bit_in_word = current_index_ & (kDataBits - 1);
      if (bit_in_word < kDataBits - 1) {
        uintptr_t remaining_bits = *ptr_ >> (bit_in_word + 1);
        if (remaining_bits) {
          int next_bit_in_word = base::bits::CountTrailingZeros(remaining_bits);
          current_index_ += next_bit_in_word + 1;
          return;
        }
      }

      // Move {current_index_} down to the beginning of the current word, before
      // starting to search for the next non-empty word.
      current_index_ = RoundDown(current_index_, kDataBits);
      do {
        ++ptr_;
        current_index_ += kDataBits;
        if (ptr_ == end_) return;
      } while (*ptr_ == 0);

      uintptr_t trailing_zeros = base::bits::CountTrailingZeros(*ptr_);
      current_index_ += trailing_zeros;
    }

   private:
    explicit Iterator(const BitVector* target, StartTag)
        : IteratorBase(target, target->data_begin_, target->data_end_, 0) {
      DCHECK_LT(ptr_, end_);
      while (*ptr_ == 0) {
        ++ptr_;
        current_index_ += kDataBits;
        if (ptr_ == end_) return;
      }
      current_index_ += base::bits::CountTrailingZeros(*ptr_);
    }

    explicit Iterator(const BitVector* target, EndTag)
        : IteratorBase(target, target->data_end_, target->data_end_,
                       target->data_length() * kDataBits) {}

    friend class BitVector;
  };  // Iterator

  // Iterates backwards (highest bit first).
  class ReverseIterator : public IteratorBase {
   public:
    V8_EXPORT_PRIVATE inline void operator++() {
      int bit_in_word = current_index_ & (kDataBits - 1);
      if (bit_in_word > 0) {
        DCHECK_NE(0, *ptr_ & (uintptr_t{1} << bit_in_word));
        uintptr_t remaining_bits = *ptr_ << (kDataBits - bit_in_word);
        if (remaining_bits) {
          int next_bit_in_word = base::bits::CountLeadingZeros(remaining_bits);
          current_index_ -= next_bit_in_word + 1;
          return;
        }
      }

      // Move {current_index_} down to the end of the next lower word, before
      // starting to search for the next non-empty word.
      current_index_ = RoundDown(current_index_, kDataBits) - 1;
      --ptr_;
      if (ptr_ == end_) return;
      while (*ptr_ == 0) {
        --ptr_;
        current_index_ -= kDataBits;
        if (ptr_ == end_) return;
      }

      uintptr_t leading_zeros = base::bits::CountLeadingZeros(*ptr_);
      current_index_ -= leading_zeros;
    }

   private:
    explicit ReverseIterator(const BitVector* target, StartTag)
        : IteratorBase(target, target->data_end_ - 1, target->data_begin_ - 1,
                       target->data_length() * kDataBits - 1) {
      DCHECK_GT(ptr_, end_);
      while (*ptr_ == 0) {
        --ptr_;
        current_index_ -= kDataBits;
        if (ptr_ == end_) return;
      }
      current_index_ -= base::bits::CountLeadingZeros(*ptr_);
    }

    explicit ReverseIterator(const BitVector* target, EndTag)
        : IteratorBase(target, target->data_begin_ - 1, target->data_begin_ - 1,
                       -1) {}

    friend class BitVector;
  };

  static constexpr int kDataBits = kBitsPerSystemPointer;
  static constexpr int kDataBitShift = kBitsPerSystemPointerLog2;

  BitVector() = default;

  BitVector(int length, Zone* zone) : length_(length) {
    DCHECK_LE(0, length);
    int data_length = (length + kDataBits - 1) >> kDataBitShift;
    if (data_length > 1) {
      data_.ptr_ = zone->AllocateArray<uintptr_t>(data_length);
      std::fill_n(data_.ptr_, data_length, 0);
      data_begin_ = data_.ptr_;
      data_end_ = data_begin_ + data_length;
    }
  }

  BitVector(const BitVector& other, Zone* zone)
      : length_(other.length_), data_(other.data_.inline_) {
    if (!other.is_inline()) {
      int data_length = other.data_length();
      DCHECK_LT(1, data_length);
      data_.ptr_ = zone->AllocateArray<uintptr_t>(data_length);
      data_begin_ = data_.ptr_;
      data_end_ = data_begin_ + data_length;
      std::copy_n(other.data_begin_, data_length, data_begin_);
    }
  }

  // Disallow copy and copy-assignment.
  BitVector(const BitVector&) = delete;
  BitVector& operator=(const BitVector&) = delete;

  BitVector(BitVector&& other) V8_NOEXCEPT { *this = std::move(other); }

  BitVector& operator=(BitVector&& other) V8_NOEXCEPT {
    length_ = other.length_;
    data_ = other.data_;
    if (other.is_inline()) {
      data_begin_ = &data_.inline_;
      data_end_ = data_begin_ + other.data_length();
    } else {
      data_begin_ = other.data_begin_;
      data_end_ = other.data_end_;
      // Reset other to inline.
      other.length_ = 0;
      other.data_begin_ = &other.data_.inline_;
      other.data_end_ = other.data_begin_ + 1;
    }
    return *this;
  }

  void CopyFrom(const BitVector& other) {
    DCHECK_EQ(other.length(), length());
    DCHECK_EQ(is_inline(), other.is_inline());
    std::copy_n(other.data_begin_, data_length(), data_begin_);
  }

  void Resize(int new_length, Zone* zone) {
    DCHECK_GT(new_length, length());
    int old_data_length = data_length();
    DCHECK_LE(1, old_data_length);
    int new_data_length = (new_length + kDataBits - 1) >> kDataBitShift;
    if (new_data_length > old_data_length) {
      uintptr_t* new_data = zone->AllocateArray<uintptr_t>(new_data_length);

      // Copy over the data.
      std::copy_n(data_begin_, old_data_length, new_data);
      // Zero out the rest of the data.
      std::fill(new_data + old_data_length, new_data + new_data_length, 0);

      data_begin_ = new_data;
      data_end_ = new_data + new_data_length;
    }
    length_ = new_length;
  }

  bool Contains(int i) const {
    DCHECK(i >= 0 && i < length());
    return (data_begin_[word(i)] & bit(i)) != 0;
  }

  void Add(int i) {
    DCHECK(i >= 0 && i < length());
    data_begin_[word(i)] |= bit(i);
  }

  void AddAll() {
    if (V8_UNLIKELY(length() == 0)) return;
    int partial_size = length() % kDataBits;
    int bulk_size = data_length() - (partial_size != 0 ? 1 : 0);
    std::fill_n(data_begin_, bulk_size, ~uintptr_t{0});
    if (partial_size != 0) {
      data_begin_[bulk_size] = ~uintptr_t{0} >> (kDataBits - partial_size);
    }
  }

  void Remove(int i) {
    DCHECK(i >= 0 && i < length());
    data_begin_[word(i)] &= ~bit(i);
  }

  void Union(const BitVector& other) {
    DCHECK_LE(other.length(), length());
    for (int i = 0; i < other.data_length(); i++) {
      data_begin_[i] |= other.data_begin_[i];
    }
  }

  bool UnionIsChanged(const BitVector& other) {
    DCHECK_LE(other.length(), length());
    bool changed = false;
    for (int i = 0; i < other.data_length(); i++) {
      uintptr_t old_data = data_begin_[i];
      data_begin_[i] |= other.data_begin_[i];
      if (data_begin_[i] != old_data) changed = true;
    }
    return changed;
  }

  void Intersect(const BitVector& other) {
    DCHECK(other.length() == length());
    for (int i = 0; i < data_length(); i++) {
      data_begin_[i] &= other.data_begin_[i];
    }
  }

  bool IntersectIsChanged(const BitVector& other) {
    DCHECK(other.length() == length());
    bool changed = false;
    for (int i = 0; i < data_length(); i++) {
      uintptr_t old_data = data_begin_[i];
      data_begin_[i] &= other.data_begin_[i];
      if (data_begin_[i] != old_data) changed = true;
    }
    return changed;
  }

  void Subtract(const BitVector& other) {
    DCHECK(other.length() == length());
    for (int i = 0; i < data_length(); i++) {
      data_begin_[i] &= ~other.data_begin_[i];
    }
  }

  bool IsSubsetOf(const BitVector& other) const {
    DCHECK(other.length() == length());
    for (int i = 0; i < data_length(); i++) {
      if ((data_begin_[i] & ~other.data_begin_[i]) != 0) return false;
    }
    return true;
  }

  void Clear() { std::fill_n(data_begin_, data_length(), 0); }

  bool IsEmpty() const {
    return std::all_of(data_begin_, data_end_, std::logical_not<uintptr_t>{});
  }

  bool Equals(const BitVector& other) const {
    return std::equal(data_begin_, data_end_, other.data_begin_);
  }

  int Count() const;

  int length() const { return length_; }

  // For overallocated vectors, finds the last bit to determine the used length.
  int UsedLength() const {
    ReverseIterator last = rbegin();
    if (last == rend()) return 0;
    return *last + 1;
  }

  Iterator begin() const { return Iterator(this, Iterator::kStartTag); }

  Iterator end() const { return Iterator(this, Iterator::kEndTag); }

  ReverseIterator rbegin() const {
    return ReverseIterator(this, ReverseIterator::kStartTag);
  }

  ReverseIterator rend() const {
    return ReverseIterator(this, ReverseIterator::kEndTag);
  }

#ifdef DEBUG
  void Print() const;
#endif

 private:
  friend V8_EXPORT_PRIVATE BitVector* CompareAndCreateXorPatch(
      Zone* zone, const GrowableBitVector& v1, const GrowableBitVector& v2,
      uint32_t* common_prefix_bits);

  union DataStorage {
    uintptr_t* ptr_;    // valid if >1 machine word is needed
    uintptr_t inline_;  // valid if <=1 machine word is needed

    explicit DataStorage(uintptr_t value) : inline_(value) {}
  };

  bool is_inline() const { return data_begin_ == &data_.inline_; }
  int data_length() const { return static_cast<int>(data_end_ - data_begin_); }

  V8_INLINE static int word(int index) {
    V8_ASSUME(index >= 0);
    return index >> kDataBitShift;
  }
  V8_INLINE static uintptr_t bit(int index) {
    return uintptr_t{1} << (index & (kDataBits - 1));
  }

  int length_ = 0;
  DataStorage data_{0};
  uintptr_t* data_begin_ = &data_.inline_;
  uintptr_t* data_end_ = &data_.inline_ + 1;
};

class GrowableBitVector {
 public:
  GrowableBitVector() = default;
  GrowableBitVector(int length, Zone* zone) : bits_(length, zone) {}

  bool Contains(int value) const {
    if (!InBitsRange(value)) return false;
    return bits_.Contains(value);
  }

  void Add(int value, Zone* zone) {
    if (V8_UNLIKELY(!InBitsRange(value))) Grow(value, zone);
    bits_.Add(value);
  }

  void Add(const GrowableBitVector& other, Zone* zone) {
    if (V8_UNLIKELY(other.length() == 0)) return;
    int max = other.length() - 1;
    if (V8_UNLIKELY(!InBitsRange(max))) Grow(max, zone);
    bits_.Union(other.bits_);
  }

  bool IsEmpty() const { return bits_.IsEmpty(); }

  void Clear() { bits_.Clear(); }

  int length() const { return bits_.length(); }

  bool Equals(const GrowableBitVector& other) const {
    return length() == other.length() && bits_.Equals(other.bits_);
  }

  BitVector::Iterator begin() const { return bits_.begin(); }

  BitVector::Iterator end() const { return bits_.end(); }

  BitVector::ReverseIterator rbegin() const { return bits_.rbegin(); }

  BitVector::ReverseIterator rend() const { return bits_.rend(); }

 private:
  friend V8_EXPORT_PRIVATE BitVector* CompareAndCreateXorPatch(
      Zone* zone, const GrowableBitVector& v1, const GrowableBitVector& v2,
      uint32_t* common_prefix_bits);

  static constexpr int kInitialLength = 1024;

  // The allocated size is always a power of two, and needs to be strictly
  // bigger than the biggest contained value.
  static constexpr int kMaxSupportedValue = (1 << 30) - 1;

  bool InBitsRange(int value) const { return bits_.length() > value; }

  V8_NOINLINE void Grow(int needed_value, Zone* zone) {
    DCHECK(!InBitsRange(needed_value));
    // Ensure that {RoundUpToPowerOfTwo32} does not overflow {int} range.
    CHECK_GE(kMaxSupportedValue, needed_value);
    int new_length = std::max(
        kInitialLength, static_cast<int>(base::bits::RoundUpToPowerOfTwo32(
                            static_cast<uint32_t>(needed_value + 1))));
    bits_.Resize(new_length, zone);
  }

  BitVector bits_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_BIT_VECTOR_H_
