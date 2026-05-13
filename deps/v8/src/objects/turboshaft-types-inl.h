// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TURBOSHAFT_TYPES_INL_H_
#define V8_OBJECTS_TURBOSHAFT_TYPES_INL_H_

#include "src/objects/turboshaft-types.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

#include "torque-generated/src/objects/turboshaft-types-tq-inl.inc"

// TurboshaftWord32RangeType
uint32_t TurboshaftWord32RangeType::from() const { return from_; }
void TurboshaftWord32RangeType::set_from(uint32_t value) { from_ = value; }
uint32_t TurboshaftWord32RangeType::to() const { return to_; }
void TurboshaftWord32RangeType::set_to(uint32_t value) { to_ = value; }

// TurboshaftWord32SetType
uint32_t TurboshaftWord32SetType::set_size() const { return set_size_; }
void TurboshaftWord32SetType::set_set_size(uint32_t value) {
  set_size_ = value;
}
uint32_t TurboshaftWord32SetType::elements(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, static_cast<int>(set_size()));
  return elements()[i];
}
void TurboshaftWord32SetType::set_elements(int i, uint32_t value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, static_cast<int>(set_size()));
  elements()[i] = value;
}

// TurboshaftWord64RangeType
uint32_t TurboshaftWord64RangeType::from_high() const { return from_high_; }
void TurboshaftWord64RangeType::set_from_high(uint32_t value) {
  from_high_ = value;
}
uint32_t TurboshaftWord64RangeType::from_low() const { return from_low_; }
void TurboshaftWord64RangeType::set_from_low(uint32_t value) {
  from_low_ = value;
}
uint32_t TurboshaftWord64RangeType::to_high() const { return to_high_; }
void TurboshaftWord64RangeType::set_to_high(uint32_t value) {
  to_high_ = value;
}
uint32_t TurboshaftWord64RangeType::to_low() const { return to_low_; }
void TurboshaftWord64RangeType::set_to_low(uint32_t value) { to_low_ = value; }

// TurboshaftWord64SetType
uint32_t TurboshaftWord64SetType::set_size() const { return set_size_; }
void TurboshaftWord64SetType::set_set_size(uint32_t value) {
  set_size_ = value;
}
uint32_t TurboshaftWord64SetType::elements_high(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, static_cast<int>(set_size()));
  return elements()[i];
}
void TurboshaftWord64SetType::set_elements_high(int i, uint32_t value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, static_cast<int>(set_size()));
  elements()[i] = value;
}
uint32_t TurboshaftWord64SetType::elements_low(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, static_cast<int>(set_size()));
  return elements()[set_size() + i];
}
void TurboshaftWord64SetType::set_elements_low(int i, uint32_t value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, static_cast<int>(set_size()));
  elements()[set_size() + i] = value;
}

// TurboshaftFloat64Type
uint32_t TurboshaftFloat64Type::special_values() const {
  return special_values_;
}
void TurboshaftFloat64Type::set_special_values(uint32_t value) {
  special_values_ = value;
}

// TurboshaftFloat64RangeType
double TurboshaftFloat64RangeType::min() const { return min_.value(); }
void TurboshaftFloat64RangeType::set_min(double value) {
  min_.set_value(value);
}
double TurboshaftFloat64RangeType::max() const { return max_.value(); }
void TurboshaftFloat64RangeType::set_max(double value) {
  max_.set_value(value);
}

// TurboshaftFloat64SetType
uint32_t TurboshaftFloat64SetType::set_size() const { return set_size_; }
void TurboshaftFloat64SetType::set_set_size(uint32_t value) {
  set_size_ = value;
}
double TurboshaftFloat64SetType::elements(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, static_cast<int>(set_size()));
  return elements()[i].value();
}
void TurboshaftFloat64SetType::set_elements(int i, double value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, static_cast<int>(set_size()));
  elements()[i].set_value(value);
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TURBOSHAFT_TYPES_INL_H_
