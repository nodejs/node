// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_DATAFLOW_H_
#define V8_DATAFLOW_H_

#include "v8.h"

#include "allocation.h"
#include "ast.h"
#include "compiler.h"
#include "zone-inl.h"

namespace v8 {
namespace internal {

class BitVector: public ZoneObject {
 public:
  // Iterator for the elements of this BitVector.
  class Iterator BASE_EMBEDDED {
   public:
    explicit Iterator(BitVector* target)
        : target_(target),
          current_index_(0),
          current_value_(target->data_[0]),
          current_(-1) {
      ASSERT(target->data_length_ > 0);
      Advance();
    }
    ~Iterator() { }

    bool Done() const { return current_index_ >= target_->data_length_; }
    void Advance();

    int Current() const {
      ASSERT(!Done());
      return current_;
    }

   private:
    uint32_t SkipZeroBytes(uint32_t val) {
      while ((val & 0xFF) == 0) {
        val >>= 8;
        current_ += 8;
      }
      return val;
    }
    uint32_t SkipZeroBits(uint32_t val) {
      while ((val & 0x1) == 0) {
        val >>= 1;
        current_++;
      }
      return val;
    }

    BitVector* target_;
    int current_index_;
    uint32_t current_value_;
    int current_;

    friend class BitVector;
  };

  BitVector(int length, Zone* zone)
      : length_(length),
        data_length_(SizeFor(length)),
        data_(zone->NewArray<uint32_t>(data_length_)) {
    ASSERT(length > 0);
    Clear();
  }

  BitVector(const BitVector& other, Zone* zone)
      : length_(other.length()),
        data_length_(SizeFor(length_)),
        data_(zone->NewArray<uint32_t>(data_length_)) {
    CopyFrom(other);
  }

  static int SizeFor(int length) {
    return 1 + ((length - 1) / 32);
  }

  BitVector& operator=(const BitVector& rhs) {
    if (this != &rhs) CopyFrom(rhs);
    return *this;
  }

  void CopyFrom(const BitVector& other) {
    ASSERT(other.length() <= length());
    for (int i = 0; i < other.data_length_; i++) {
      data_[i] = other.data_[i];
    }
    for (int i = other.data_length_; i < data_length_; i++) {
      data_[i] = 0;
    }
  }

  bool Contains(int i) const {
    ASSERT(i >= 0 && i < length());
    uint32_t block = data_[i / 32];
    return (block & (1U << (i % 32))) != 0;
  }

  void Add(int i) {
    ASSERT(i >= 0 && i < length());
    data_[i / 32] |= (1U << (i % 32));
  }

  void Remove(int i) {
    ASSERT(i >= 0 && i < length());
    data_[i / 32] &= ~(1U << (i % 32));
  }

  void Union(const BitVector& other) {
    ASSERT(other.length() == length());
    for (int i = 0; i < data_length_; i++) {
      data_[i] |= other.data_[i];
    }
  }

  bool UnionIsChanged(const BitVector& other) {
    ASSERT(other.length() == length());
    bool changed = false;
    for (int i = 0; i < data_length_; i++) {
      uint32_t old_data = data_[i];
      data_[i] |= other.data_[i];
      if (data_[i] != old_data) changed = true;
    }
    return changed;
  }

  void Intersect(const BitVector& other) {
    ASSERT(other.length() == length());
    for (int i = 0; i < data_length_; i++) {
      data_[i] &= other.data_[i];
    }
  }

  void Subtract(const BitVector& other) {
    ASSERT(other.length() == length());
    for (int i = 0; i < data_length_; i++) {
      data_[i] &= ~other.data_[i];
    }
  }

  void Clear() {
    for (int i = 0; i < data_length_; i++) {
      data_[i] = 0;
    }
  }

  bool IsEmpty() const {
    for (int i = 0; i < data_length_; i++) {
      if (data_[i] != 0) return false;
    }
    return true;
  }

  bool Equals(const BitVector& other) {
    for (int i = 0; i < data_length_; i++) {
      if (data_[i] != other.data_[i]) return false;
    }
    return true;
  }

  int length() const { return length_; }

#ifdef DEBUG
  void Print();
#endif

 private:
  int length_;
  int data_length_;
  uint32_t* data_;
};

class GrowableBitVector BASE_EMBEDDED {
 public:
  class Iterator BASE_EMBEDDED {
   public:
    Iterator(const GrowableBitVector* target, Zone* zone)
        : it_(target->bits_ == NULL
              ? new(zone) BitVector(1, zone)
              : target->bits_) { }
    bool Done() const { return it_.Done(); }
    void Advance() { it_.Advance(); }
    int Current() const { return it_.Current(); }
   private:
    BitVector::Iterator it_;
  };

  GrowableBitVector() : bits_(NULL) { }
  GrowableBitVector(int length, Zone* zone)
      : bits_(new(zone) BitVector(length, zone)) { }

  bool Contains(int value) const {
    if (!InBitsRange(value)) return false;
    return bits_->Contains(value);
  }

  void Add(int value, Zone* zone) {
    EnsureCapacity(value, zone);
    bits_->Add(value);
  }

  void Union(const GrowableBitVector& other, Zone* zone) {
    for (Iterator it(&other, zone); !it.Done(); it.Advance()) {
      Add(it.Current(), zone);
    }
  }

  void Clear() { if (bits_ != NULL) bits_->Clear(); }

 private:
  static const int kInitialLength = 1024;

  bool InBitsRange(int value) const {
    return bits_ != NULL && bits_->length() > value;
  }

  void EnsureCapacity(int value, Zone* zone) {
    if (InBitsRange(value)) return;
    int new_length = bits_ == NULL ? kInitialLength : bits_->length();
    while (new_length <= value) new_length *= 2;
    BitVector* new_bits = new(zone) BitVector(new_length, zone);
    if (bits_ != NULL) new_bits->CopyFrom(*bits_);
    bits_ = new_bits;
  }

  BitVector* bits_;
};


} }  // namespace v8::internal


#endif  // V8_DATAFLOW_H_
