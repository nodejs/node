// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BYTECODE_LIVENESS_MAP_H_
#define V8_COMPILER_BYTECODE_LIVENESS_MAP_H_

#include <string>

#include "src/utils/bit-vector.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class Zone;

namespace compiler {

class BytecodeLivenessState : public ZoneObject {
 public:
  class Iterator {
   public:
    int operator*() const {
      // Subtract one to compensate for the accumulator at the start of the
      // bit vector.
      return *it_ - 1;
    }

    void operator++() { return ++it_; }

    bool operator!=(const Iterator& other) const { return it_ != other.it_; }

   private:
    static constexpr struct StartTag {
    } kStartTag = {};
    static constexpr struct EndTag {
    } kEndTag = {};
    explicit Iterator(const BytecodeLivenessState& liveness, StartTag)
        : it_(liveness.bit_vector_.begin()) {
      // If we're not at the end, and the current value is the accumulator, skip
      // over it.
      if (it_ != liveness.bit_vector_.end() && *it_ == 0) {
        ++it_;
      }
    }
    explicit Iterator(const BytecodeLivenessState& liveness, EndTag)
        : it_(liveness.bit_vector_.end()) {}

    BitVector::Iterator it_;
    friend class BytecodeLivenessState;
  };

  BytecodeLivenessState(int register_count, Zone* zone)
      : bit_vector_(register_count + 1, zone) {}
  BytecodeLivenessState(const BytecodeLivenessState&) = delete;
  BytecodeLivenessState& operator=(const BytecodeLivenessState&) = delete;

  BytecodeLivenessState(const BytecodeLivenessState& other, Zone* zone)
      : bit_vector_(other.bit_vector_, zone) {}

  bool RegisterIsLive(int index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, bit_vector_.length() - 1);
    return bit_vector_.Contains(index + 1);
  }

  bool AccumulatorIsLive() const { return bit_vector_.Contains(0); }

  bool Equals(const BytecodeLivenessState& other) const {
    return bit_vector_.Equals(other.bit_vector_);
  }

  void MarkRegisterLive(int index) {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, bit_vector_.length() - 1);
    bit_vector_.Add(index + 1);
  }

  void MarkRegisterDead(int index) {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, bit_vector_.length() - 1);
    bit_vector_.Remove(index + 1);
  }

  void MarkAccumulatorLive() { bit_vector_.Add(0); }

  void MarkAccumulatorDead() { bit_vector_.Remove(0); }

  void MarkAllLive() { bit_vector_.AddAll(); }

  void Union(const BytecodeLivenessState& other) {
    bit_vector_.Union(other.bit_vector_);
  }

  bool UnionIsChanged(const BytecodeLivenessState& other) {
    return bit_vector_.UnionIsChanged(other.bit_vector_);
  }

  void CopyFrom(const BytecodeLivenessState& other) {
    bit_vector_.CopyFrom(other.bit_vector_);
  }

  int register_count() const { return bit_vector_.length() - 1; }

  // Number of live values, including the accumulator.
  int live_value_count() const { return bit_vector_.Count(); }

  Iterator begin() const { return Iterator(*this, Iterator::kStartTag); }

  Iterator end() const { return Iterator(*this, Iterator::kEndTag); }

 private:
  BitVector bit_vector_;
};

struct BytecodeLiveness {
  BytecodeLivenessState* in;
  BytecodeLivenessState* out;
};

class V8_EXPORT_PRIVATE BytecodeLivenessMap {
 public:
  BytecodeLivenessMap(int bytecode_size, Zone* zone)
      : liveness_(zone->AllocateArray<BytecodeLiveness>(bytecode_size))
#ifdef DEBUG
        ,
        size_(bytecode_size)
#endif
  {
  }

  BytecodeLiveness& InsertNewLiveness(int offset) {
    DCHECK_GE(offset, 0);
    DCHECK_LT(offset, size_);
#ifdef DEBUG
    // Null out the in/out liveness, so that later DCHECKs know whether these
    // have been correctly initialised or not. That code does initialise them
    // unconditionally though, so we can skip the nulling out in release.
    liveness_[offset].in = nullptr;
    liveness_[offset].out = nullptr;
#endif
    return liveness_[offset];
  }

  BytecodeLiveness& GetLiveness(int offset) {
    DCHECK_GE(offset, 0);
    DCHECK_LT(offset, size_);
    return liveness_[offset];
  }
  const BytecodeLiveness& GetLiveness(int offset) const {
    DCHECK_GE(offset, 0);
    DCHECK_LT(offset, size_);
    return liveness_[offset];
  }

  BytecodeLivenessState* GetInLiveness(int offset) {
    return GetLiveness(offset).in;
  }
  const BytecodeLivenessState* GetInLiveness(int offset) const {
    return GetLiveness(offset).in;
  }

  BytecodeLivenessState* GetOutLiveness(int offset) {
    return GetLiveness(offset).out;
  }
  const BytecodeLivenessState* GetOutLiveness(int offset) const {
    return GetLiveness(offset).out;
  }

 private:
  BytecodeLiveness* liveness_;
#ifdef DEBUG
  size_t size_;
#endif
};

V8_EXPORT_PRIVATE std::string ToString(const BytecodeLivenessState& liveness);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BYTECODE_LIVENESS_MAP_H_
