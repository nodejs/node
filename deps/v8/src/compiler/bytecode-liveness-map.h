// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BYTECODE_LIVENESS_MAP_H_
#define V8_COMPILER_BYTECODE_LIVENESS_MAP_H_

#include "src/base/hashmap.h"
#include "src/bit-vector.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class Zone;

namespace compiler {

class BytecodeLivenessState : public ZoneObject {
 public:
  BytecodeLivenessState(int register_count, Zone* zone)
      : bit_vector_(register_count + 1, zone) {}

  const BitVector& bit_vector() const { return bit_vector_; }

  BitVector& bit_vector() { return bit_vector_; }

  bool RegisterIsLive(int index) const {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, bit_vector_.length() - 1);
    return bit_vector_.Contains(index);
  }

  bool AccumulatorIsLive() const {
    return bit_vector_.Contains(bit_vector_.length() - 1);
  }

  bool Equals(const BytecodeLivenessState& other) const {
    return bit_vector_.Equals(other.bit_vector_);
  }

  void MarkRegisterLive(int index) {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, bit_vector_.length() - 1);
    bit_vector_.Add(index);
  }

  void MarkRegisterDead(int index) {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, bit_vector_.length() - 1);
    bit_vector_.Remove(index);
  }

  void MarkAccumulatorLive() { bit_vector_.Add(bit_vector_.length() - 1); }

  void MarkAccumulatorDead() { bit_vector_.Remove(bit_vector_.length() - 1); }

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

 private:
  BitVector bit_vector_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeLivenessState);
};

struct BytecodeLiveness {
  BytecodeLivenessState* in;
  BytecodeLivenessState* out;

  BytecodeLiveness(int register_count, Zone* zone);
};

class V8_EXPORT_PRIVATE BytecodeLivenessMap {
 public:
  BytecodeLivenessMap(int size, Zone* zone);

  BytecodeLiveness& InitializeLiveness(int offset, int register_count,
                                       Zone* zone);

  BytecodeLiveness& GetLiveness(int offset);
  const BytecodeLiveness& GetLiveness(int offset) const;

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
  base::TemplateHashMapImpl<int, BytecodeLiveness,
                            base::KeyEqualityMatcher<int>, ZoneAllocationPolicy>
      liveness_map_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BYTECODE_LIVENESS_MAP_H_
