// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_BIT_VECTOR_H_
#define V8_UTILS_BIT_VECTOR_H_

#include "src/base/bits.h"
#include "src/utils/allocation.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE BitVector : public ZoneObject {
 public:
  union DataStorage {
    uintptr_t* ptr_;    // valid if data_length_ > 1
    uintptr_t inline_;  // valid if data_length_ == 1

    explicit DataStorage(uintptr_t value) : inline_(value) {}
  };

  // Iterator for the elements of this BitVector.
  class Iterator {
   public:
    V8_EXPORT_PRIVATE inline void operator++() {
      current_++;

      // Skip zeroed words.
      while (current_value_ == 0) {
        current_index_++;
        if (Done()) return;
        DCHECK(!target_->is_inline());
        current_value_ = target_->data_.ptr_[current_index_];
        current_ = current_index_ << kDataBitShift;
      }

      // Skip zeroed bits.
      uintptr_t trailing_zeros = base::bits::CountTrailingZeros(current_value_);
      current_ += trailing_zeros;
      current_value_ >>= trailing_zeros;

      // Get current_value ready for next advance.
      current_value_ >>= 1;
    }

    int operator*() const {
      DCHECK(!Done());
      return current_;
    }

    bool operator!=(const Iterator& other) const {
      // "other" is required to be the end sentinel value, to avoid us needing
      // to compare exact "current" values.
      DCHECK(other.Done());
      DCHECK_EQ(target_, other.target_);
      return current_index_ != other.current_index_;
    }

   private:
    static constexpr struct StartTag {
    } kStartTag = {};
    static constexpr struct EndTag {
    } kEndTag = {};

    explicit Iterator(const BitVector* target, StartTag)
        : target_(target),
          current_value_(target->is_inline() ? target->data_.inline_
                                             : target->data_.ptr_[0]),
          current_index_(0),
          current_(-1) {
      ++(*this);
    }
    explicit Iterator(const BitVector* target, EndTag)
        : target_(target),
          current_value_(0),
          current_index_(target->data_length_),
          current_(-1) {
      DCHECK(Done());
    }

    bool Done() const { return current_index_ >= target_->data_length_; }

    const BitVector* target_;
    uintptr_t current_value_;
    int current_index_;
    int current_;

    friend class BitVector;
  };

  static const int kDataLengthForInline = 1;
  static const int kDataBits = kBitsPerSystemPointer;
  static const int kDataBitShift = kBitsPerSystemPointerLog2;
  static const uintptr_t kOne = 1;  // This saves some static_casts.

  BitVector() : length_(0), data_length_(kDataLengthForInline), data_(0) {}

  BitVector(int length, Zone* zone)
      : length_(length), data_length_(SizeFor(length)), data_(0) {
    DCHECK_LE(0, length);
    if (!is_inline()) {
      data_.ptr_ = zone->NewArray<uintptr_t>(data_length_);
      Clear();
    }
    // Otherwise, clearing is implicit
  }

  BitVector(const BitVector& other, Zone* zone)
      : length_(other.length_),
        data_length_(other.data_length_),
        data_(other.data_.inline_) {
    if (!is_inline()) {
      data_.ptr_ = zone->NewArray<uintptr_t>(data_length_);
      for (int i = 0; i < other.data_length_; i++) {
        data_.ptr_[i] = other.data_.ptr_[i];
      }
    }
  }

  static int SizeFor(int length) {
    if (length <= kDataBits) {
      return kDataLengthForInline;
    }

    int data_length = 1 + ((length - 1) / kDataBits);
    DCHECK_GT(data_length, kDataLengthForInline);
    return data_length;
  }

  void CopyFrom(const BitVector& other) {
    DCHECK_EQ(other.length(), length());
    if (is_inline()) {
      DCHECK(other.is_inline());
      data_.inline_ = other.data_.inline_;
    } else {
      for (int i = 0; i < data_length_; i++) {
        data_.ptr_[i] = other.data_.ptr_[i];
      }
    }
  }

  void Resize(int new_length, Zone* zone) {
    DCHECK_GT(new_length, length());
    int new_data_length = SizeFor(new_length);
    if (new_data_length > data_length_) {
      DataStorage old_data = data_;
      int old_data_length = data_length_;

      // Make sure the new data length is large enough to need allocation.
      DCHECK_GT(new_data_length, kDataLengthForInline);
      data_.ptr_ = zone->NewArray<uintptr_t>(new_data_length);
      data_length_ = new_data_length;

      // Copy over the data.
      if (old_data_length == kDataLengthForInline) {
        data_.ptr_[0] = old_data.inline_;
      } else {
        for (int i = 0; i < old_data_length; i++) {
          data_.ptr_[i] = old_data.ptr_[i];
        }
      }
      // Zero out the rest of the data.
      for (int i = old_data_length; i < data_length_; i++) {
        data_.ptr_[i] = 0;
      }
    }
    length_ = new_length;
  }

  bool Contains(int i) const {
    DCHECK(i >= 0 && i < length());
    uintptr_t block = is_inline() ? data_.inline_ : data_.ptr_[i / kDataBits];
    return (block & (kOne << (i % kDataBits))) != 0;
  }

  void Add(int i) {
    DCHECK(i >= 0 && i < length());
    if (is_inline()) {
      data_.inline_ |= (kOne << i);
    } else {
      data_.ptr_[i / kDataBits] |= (kOne << (i % kDataBits));
    }
  }

  void AddAll() {
    // TODO(leszeks): This sets bits outside of the length of this bit-vector,
    // which is observable if we resize it or copy from it. If this is a
    // problem, we should clear the high bits either on add, or on resize/copy.
    if (is_inline()) {
      data_.inline_ = -1;
    } else {
      memset(data_.ptr_, -1, sizeof(uintptr_t) * data_length_);
    }
  }

  void Remove(int i) {
    DCHECK(i >= 0 && i < length());
    if (is_inline()) {
      data_.inline_ &= ~(kOne << i);
    } else {
      data_.ptr_[i / kDataBits] &= ~(kOne << (i % kDataBits));
    }
  }

  void Union(const BitVector& other) {
    DCHECK(other.length() == length());
    if (is_inline()) {
      DCHECK(other.is_inline());
      data_.inline_ |= other.data_.inline_;
    } else {
      for (int i = 0; i < data_length_; i++) {
        data_.ptr_[i] |= other.data_.ptr_[i];
      }
    }
  }

  bool UnionIsChanged(const BitVector& other) {
    DCHECK(other.length() == length());
    if (is_inline()) {
      DCHECK(other.is_inline());
      uintptr_t old_data = data_.inline_;
      data_.inline_ |= other.data_.inline_;
      return data_.inline_ != old_data;
    } else {
      bool changed = false;
      for (int i = 0; i < data_length_; i++) {
        uintptr_t old_data = data_.ptr_[i];
        data_.ptr_[i] |= other.data_.ptr_[i];
        if (data_.ptr_[i] != old_data) changed = true;
      }
      return changed;
    }
  }

  void Intersect(const BitVector& other) {
    DCHECK(other.length() == length());
    if (is_inline()) {
      DCHECK(other.is_inline());
      data_.inline_ &= other.data_.inline_;
    } else {
      for (int i = 0; i < data_length_; i++) {
        data_.ptr_[i] &= other.data_.ptr_[i];
      }
    }
  }

  bool IntersectIsChanged(const BitVector& other) {
    DCHECK(other.length() == length());
    if (is_inline()) {
      DCHECK(other.is_inline());
      uintptr_t old_data = data_.inline_;
      data_.inline_ &= other.data_.inline_;
      return data_.inline_ != old_data;
    } else {
      bool changed = false;
      for (int i = 0; i < data_length_; i++) {
        uintptr_t old_data = data_.ptr_[i];
        data_.ptr_[i] &= other.data_.ptr_[i];
        if (data_.ptr_[i] != old_data) changed = true;
      }
      return changed;
    }
  }

  void Subtract(const BitVector& other) {
    DCHECK(other.length() == length());
    if (is_inline()) {
      DCHECK(other.is_inline());
      data_.inline_ &= ~other.data_.inline_;
    } else {
      for (int i = 0; i < data_length_; i++) {
        data_.ptr_[i] &= ~other.data_.ptr_[i];
      }
    }
  }

  void Clear() {
    if (is_inline()) {
      data_.inline_ = 0;
    } else {
      for (int i = 0; i < data_length_; i++) {
        data_.ptr_[i] = 0;
      }
    }
  }

  bool IsEmpty() const {
    if (is_inline()) {
      return data_.inline_ == 0;
    } else {
      for (int i = 0; i < data_length_; i++) {
        if (data_.ptr_[i] != 0) return false;
      }
      return true;
    }
  }

  bool Equals(const BitVector& other) const {
    DCHECK(other.length() == length());
    if (is_inline()) {
      DCHECK(other.is_inline());
      return data_.inline_ == other.data_.inline_;
    } else {
      for (int i = 0; i < data_length_; i++) {
        if (data_.ptr_[i] != other.data_.ptr_[i]) return false;
      }
      return true;
    }
  }

  int Count() const;

  int length() const { return length_; }

  Iterator begin() const { return Iterator(this, Iterator::kStartTag); }

  Iterator end() const { return Iterator(this, Iterator::kEndTag); }

#ifdef DEBUG
  void Print() const;
#endif

  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(BitVector);

 private:
  int length_;
  int data_length_;
  DataStorage data_;

  bool is_inline() const { return data_length_ == kDataLengthForInline; }
};

class GrowableBitVector {
 public:
  GrowableBitVector() : bits_() {}
  GrowableBitVector(int length, Zone* zone) : bits_(length, zone) {}

  bool Contains(int value) const {
    if (!InBitsRange(value)) return false;
    return bits_.Contains(value);
  }

  void Add(int value, Zone* zone) {
    EnsureCapacity(value, zone);
    bits_.Add(value);
  }

  void Clear() { bits_.Clear(); }

  int length() const { return bits_.length(); }

  bool Equals(const GrowableBitVector& other) const {
    return length() == other.length() && bits_.Equals(other.bits_);
  }

  BitVector::Iterator begin() const { return bits_.begin(); }

  BitVector::Iterator end() const { return bits_.end(); }

 private:
  static constexpr int kInitialLength = 1024;

  bool InBitsRange(int value) const { return bits_.length() > value; }

  void EnsureCapacity(int value, Zone* zone) {
    if (InBitsRange(value)) return;
    int new_length =
        base::bits::RoundUpToPowerOfTwo32(static_cast<uint32_t>(value));
    new_length = std::min(new_length, kInitialLength);
    while (new_length <= value) new_length *= 2;
    bits_.Resize(new_length, zone);
  }

  BitVector bits_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_BIT_VECTOR_H_
