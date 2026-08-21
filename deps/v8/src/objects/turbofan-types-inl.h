// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TURBOFAN_TYPES_INL_H_
#define V8_OBJECTS_TURBOFAN_TYPES_INL_H_

#include "src/objects/turbofan-types.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap-write-barrier.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/turbofan-types-tq-inl.inc"

// TurbofanBitsetType
uint32_t TurbofanBitsetType::bitset_low() const { return bitset_low_; }
void TurbofanBitsetType::set_bitset_low(uint32_t value) { bitset_low_ = value; }
uint32_t TurbofanBitsetType::bitset_high() const { return bitset_high_; }
void TurbofanBitsetType::set_bitset_high(uint32_t value) {
  bitset_high_ = value;
}

// TurbofanUnionType
Tagged<TurbofanType> TurbofanUnionType::type1() const { return type1_.load(); }
void TurbofanUnionType::set_type1(Tagged<TurbofanType> value,
                                  WriteBarrierMode mode) {
  type1_.store(this, value, mode);
}
Tagged<TurbofanType> TurbofanUnionType::type2() const { return type2_.load(); }
void TurbofanUnionType::set_type2(Tagged<TurbofanType> value,
                                  WriteBarrierMode mode) {
  type2_.store(this, value, mode);
}

// TurbofanRangeType
double TurbofanRangeType::min() const { return min_.value(); }
void TurbofanRangeType::set_min(double value) { min_.set_value(value); }
double TurbofanRangeType::max() const { return max_.value(); }
void TurbofanRangeType::set_max(double value) { max_.set_value(value); }

// TurbofanHeapConstantType
Tagged<HeapObject> TurbofanHeapConstantType::constant() const {
  return Cast<HeapObject>(constant_.load());
}
void TurbofanHeapConstantType::set_constant(Tagged<HeapObject> value,
                                            WriteBarrierMode mode) {
  constant_.store(this, value, mode);
}

// TurbofanOtherNumberConstantType
double TurbofanOtherNumberConstantType::constant() const {
  return constant_.value();
}
void TurbofanOtherNumberConstantType::set_constant(double value) {
  constant_.set_value(value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TURBOFAN_TYPES_INL_H_
