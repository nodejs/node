// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FLOAT32_H_
#define V8_FLOAT32_H_

#include "src/base/macros.h"

namespace v8 {
namespace internal {

// TODO(ahaas): Make these classes with the one in double.h

// Safety wrapper for a 32-bit floating-point value to make sure we don't lose
// the exact bit pattern during deoptimization when passing this value.
class Float32 {
 public:
  Float32() : bit_pattern_(0) {}

  explicit Float32(uint32_t bit_pattern) : bit_pattern_(bit_pattern) {}

  // This constructor does not guarantee that bit pattern of the input value
  // is preserved if the input is a NaN.
  explicit Float32(float value) : bit_pattern_(bit_cast<uint32_t>(value)) {}

  uint32_t get_bits() const { return bit_pattern_; }

  float get_scalar() const { return bit_cast<float>(bit_pattern_); }

  static Float32 FromBits(uint32_t bits) { return Float32(bits); }

 private:
  uint32_t bit_pattern_;
};

// Safety wrapper for a 64-bit floating-point value to make sure we don't lose
// the exact bit pattern during deoptimization when passing this value. Note
// that there is intentionally no way to construct it from a {double} value.
// TODO(ahaas): Unify this class with Double in double.h
class Float64 {
 public:
  Float64() : bit_pattern_(0) {}
  uint64_t get_bits() const { return bit_pattern_; }
  double get_scalar() const { return bit_cast<double>(bit_pattern_); }
  bool is_hole_nan() const { return bit_pattern_ == kHoleNanInt64; }
  static Float64 FromBits(uint64_t bits) { return Float64(bits); }

 private:
  explicit Float64(uint64_t bit_pattern) : bit_pattern_(bit_pattern) {}
  uint64_t bit_pattern_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_FLOAT32_H_
