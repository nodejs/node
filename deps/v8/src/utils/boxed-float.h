// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_BOXED_FLOAT_H_
#define V8_UTILS_BOXED_FLOAT_H_

#include <cmath>

#include "absl/strings/str_format.h"
#include "src/base/hashing.h"
#include "src/base/macros.h"
#include "src/base/numbers/double.h"
#include "src/common/globals.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

// TODO(ahaas): Make these classes with the one in double.h

// Safety wrapper for a 32-bit floating-point value to make sure we don't lose
// the exact bit pattern during deoptimization when passing this value.
class Float32 {
 public:
  constexpr Float32() = default;

  // This constructor does not guarantee that bit pattern of the input value
  // is preserved if the input is a NaN.
  explicit Float32(float value)
      : bit_pattern_(base::bit_cast<uint32_t>(value)) {
    // Check that the provided value is not a NaN, because the bit pattern of a
    // NaN may be changed by a base::bit_cast, e.g. for signalling NaNs on
    // ia32.
    DCHECK(!std::isnan(value));
  }

  constexpr uint32_t get_bits() const { return bit_pattern_; }

  float get_scalar() const { return base::bit_cast<float>(bit_pattern_); }

  bool is_nan() const {
    // Even though {get_scalar()} might set the quiet NaN bit, it's ok here,
    // because this does not change the is_nan property.
    bool nan = std::isnan(get_scalar());
    DCHECK_EQ(nan, exponent() == 0xff && mantissa() != 0);
    return nan;
  }

  bool is_quiet_nan() const {
    return is_nan() && (bit_pattern_ & kQuietNanBit);
  }

  bool is_inf() const {
    bool inf = std::isinf(get_scalar());
    DCHECK_EQ(inf, exponent() == 0xff && mantissa() == 0);
    return inf;
  }

  constexpr bool is_negative() const { return bit_pattern_ & kSignBit; }

  V8_WARN_UNUSED_RESULT Float32 to_quiet_nan() const {
    DCHECK(is_nan());
    Float32 quiet_nan{bit_pattern_ | kQuietNanBit};
    DCHECK(quiet_nan.is_quiet_nan());
    return quiet_nan;
  }

  V8_WARN_UNUSED_RESULT Float32 to_negative() const {
    return Float32::FromBits(bit_pattern_ | kSignBit);
  }

  constexpr bool operator==(const Float32&) const = default;

  constexpr Float32 operator-() const {
    return Float32::FromBits(bit_pattern_ ^ kSignBit);
  }

  static constexpr Float32 FromBits(uint32_t bits) { return Float32(bits); }

  // This static constructor allows passing NaNs, but as the signalling bit
  // might get lost when passing the float parameter we explicitly put this in
  // the method name, and still DCHECK for it (which might not trigger if a
  // signalling NaN is implicitly silenced when passed here).
  static Float32 FromNonSignallingFloat(float value) {
    Float32 result = Float32::FromBits(base::bit_cast<uint32_t>(value));
    DCHECK(!result.is_nan() || result.is_quiet_nan());
    return result;
  }

  static constexpr Float32 quiet_nan() {
    return FromBits((0xffu << kExponentShift) | kQuietNanBit);
  }

  static constexpr Float32 infinity() {
    return FromBits(0xffu << kExponentShift);
  }

  // absl stringify support, enabling e.g. FuzzTest outputting Float32 values
  // instead of printing "unprintable value".
  template <typename Sink>
  friend void AbslStringify(Sink& sink, Float32 f) {
    absl::Format(&sink, "%f (0x%08x)", f.get_scalar(), f.get_bits());
  }

 private:
  explicit constexpr Float32(uint32_t bit_pattern)
      : bit_pattern_(bit_pattern) {}

  static constexpr int kExponentShift = 23;
  static constexpr uint32_t kQuietNanBit = 1 << 22;
  static constexpr uint32_t kSignBit = 1 << 31;

  uint32_t exponent() const { return (bit_pattern_ >> kExponentShift) & 0xff; }
  uint32_t mantissa() const {
    return bit_pattern_ & ((1 << kExponentShift) - 1);
  }

  uint32_t bit_pattern_ = 0;
};

ASSERT_TRIVIALLY_COPYABLE(Float32);

inline std::ostream& operator<<(std::ostream& os, const Float32& float32) {
  return os << float32.get_scalar() << " ("
            << AsHex(float32.get_bits(), 8, true) << ")";
}

// Safety wrapper for a 64-bit floating-point value to make sure we don't lose
// the exact bit pattern during deoptimization when passing this value.
// TODO(ahaas): Unify this class with Double in double.h
class Float64 {
 public:
  Float64() = default;

  // This constructor does not guarantee that bit pattern of the input value
  // is preserved if the input is a NaN.
  explicit Float64(double value)
      : bit_pattern_(base::bit_cast<uint64_t>(value)) {
    // Check that the provided value is not a NaN, because the bit pattern of a
    // NaN may be changed by a base::bit_cast, e.g. for signalling NaNs on
    // ia32.
    DCHECK(!std::isnan(value));
  }

  explicit Float64(base::Double value) : bit_pattern_(value.AsUint64()) {}

  static constexpr Float64 quiet_nan() {
    return Float64::FromBits(
        base::double_to_uint64(std::numeric_limits<double>::quiet_NaN()));
  }
  static constexpr Float64 hole_nan() {
    return Float64::FromBits(kHoleNanInt64);
  }
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
  static constexpr Float64 undefined_nan() {
    return Float64::FromBits(kUndefinedNanInt64);
  }
#endif

  uint64_t get_bits() const { return bit_pattern_; }
  double get_scalar() const { return base::bit_cast<double>(bit_pattern_); }
  bool is_hole_nan() const { return bit_pattern_ == kHoleNanInt64; }
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
  bool is_undefined_nan() const { return bit_pattern_ == kUndefinedNanInt64; }
#endif  // V8_ENABLE_UNDEFINED_DOUBLE
  bool is_undefined_or_hole_nan() const {
    return
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
        is_undefined_nan() ||
#endif
        is_hole_nan();
  }

  bool is_nan() const {
    // Even though {get_scalar()} might set the quiet NaN bit, it's ok here,
    // because this does not change the is_nan property.
    bool nan = std::isnan(get_scalar());
    DCHECK_EQ(nan, exponent() == 0x7ff && mantissa() != 0);
    return nan;
  }

  bool is_quiet_nan() const {
    return is_nan() && (bit_pattern_ & (uint64_t{1} << 51));
  }

  V8_WARN_UNUSED_RESULT Float64 to_quiet_nan() const {
    DCHECK(is_nan());
    Float64 quiet_nan{bit_pattern_ | (uint64_t{1} << 51)};
    DCHECK(quiet_nan.is_quiet_nan());
    return quiet_nan;
  }

  static constexpr Float64 FromBits(uint64_t bits) { return Float64(bits); }

  constexpr bool operator==(const Float64&) const = default;

  size_t hash_value() const { return base::hash_value(bit_pattern_); }

 private:
  explicit constexpr Float64(uint64_t bit_pattern)
      : bit_pattern_(bit_pattern) {}

  uint64_t exponent() const { return (bit_pattern_ >> 52) & ((1 << 11) - 1); }
  uint64_t mantissa() const { return bit_pattern_ & ((uint64_t{1} << 52) - 1); }

  uint64_t bit_pattern_ = 0;
};

ASSERT_TRIVIALLY_COPYABLE(Float64);

inline std::ostream& operator<<(std::ostream& os, const Float64& float64) {
  return os << float64.get_scalar() << " ("
            << AsHex(float64.get_bits(), 16, true) << ")";
}

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_BOXED_FLOAT_H_
