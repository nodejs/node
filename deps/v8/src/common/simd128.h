// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_SIMD128_H_
#define V8_COMMON_SIMD128_H_

#include <cstdint>

#include "absl/strings/str_format.h"
#include "src/base/macros.h"
#include "src/common/globals.h"

namespace v8::internal {

#define FOREACH_SIMD_TYPE(V)     \
  V(double, float64x2, f64x2, 2) \
  V(float, float32x4, f32x4, 4)  \
  V(int64_t, int64x2, i64x2, 2)  \
  V(int32_t, int32x4, i32x4, 4)  \
  V(int16_t, int16x8, i16x8, 8)  \
  V(int8_t, int8x16, i8x16, 16)

class alignas(double) Simd128 {
 public:
#define DEFINE_SIMD_TYPE(cType, sType, name, kSize) \
  using sType = std::array<cType, kSize>;
  FOREACH_SIMD_TYPE(DEFINE_SIMD_TYPE)
#undef DEFINE_SIMD_TYPE

  constexpr Simd128() = default;

#define DEFINE_SIMD_TYPE_SPECIFIC_METHODS(cType, sType, name, size) \
  explicit constexpr Simd128(sType val)                             \
      : val_(base::bit_cast<std::array<uint8_t, 16>>(val)) {}       \
  static constexpr Simd128 Splat(cType value) {                     \
    sType values;                                                   \
    std::fill_n(values.data(), size, value);                        \
    return Simd128{values};                                         \
  }                                                                 \
  constexpr sType to_##name() const { return base::bit_cast<sType>(val_); }
  FOREACH_SIMD_TYPE(DEFINE_SIMD_TYPE_SPECIFIC_METHODS)
#undef DEFINE_SIMD_TYPE_SPECIFIC_METHODS

  explicit Simd128(uint8_t* bytes) { memcpy(val_.data(), bytes, sizeof(val_)); }

  bool operator==(const Simd128&) const = default;

  const uint8_t* bytes() const { return val_.data(); }

  // absl stringify support, enabling e.g. FuzzTest outputting Simd128 values
  // instead of printing "unprintable value".
  template <typename Sink>
  friend void AbslStringify(Sink& sink, Simd128 value) {
    float64x2 f64x2 = value.to_f64x2();
    float32x4 f32x4 = value.to_f32x4();
    int64x2 i64x2 = value.to_i64x2();
    int32x4 i32x4 = value.to_i32x4();
    int16x8 i16x8 = value.to_i16x8();
    int8x16 i8x16 = value.to_i8x16();
    absl::Format(&sink,
                 "Simd128 {\n"
                 "  f64x2: %g, %g\n"
                 "  f32x4: %g, %g, %g, %g\n"
                 "  i64x2: %d (0x%016x), %d (0x%016x)\n"
                 "  i32x4: %d (0x%08x), %d (0x%08x), %d (0x%08x), %d (0x%08x)\n"
                 "  i16x8: %d (0x%04x), %d (0x%04x), %d (0x%04x), %d (0x%04x), "
                 "%d (0x%04x), %d (0x%04x), %d (0x%04x), %d (0x%04x)\n"
                 "  i8x16: %d (0x%02x), %d (0x%02x), %d (0x%02x), %d (0x%02x), "
                 "%d (0x%02x), %d (0x%02x), %d (0x%02x), %d (0x%02x), "
                 "%d (0x%02x), %d (0x%02x), %d (0x%02x), %d (0x%02x), "
                 "%d (0x%02x), %d (0x%02x), %d (0x%02x), %d (0x%02x)\n"
                 "}",
                 // f64x2
                 f64x2[0], f64x2[1],
                 // f32x4
                 f32x4[0], f32x4[1], f32x4[2], f32x4[3],
                 // i64x2
                 i64x2[0], i64x2[0], i64x2[1], i64x2[1],
                 // i32x4
                 i32x4[0], i32x4[0], i32x4[1], i32x4[1], i32x4[2], i32x4[2],
                 i32x4[3], i32x4[3],
                 // i16x8
                 i16x8[0], i16x8[0], i16x8[1], i16x8[1], i16x8[2], i16x8[2],
                 i16x8[3], i16x8[3], i16x8[4], i16x8[4], i16x8[5], i16x8[5],
                 i16x8[6], i16x8[6], i16x8[7], i16x8[7],
                 // i8x16
                 i8x16[0], i8x16[0], i8x16[1], i8x16[1], i8x16[2], i8x16[2],
                 i8x16[3], i8x16[3], i8x16[4], i8x16[4], i8x16[5], i8x16[5],
                 i8x16[6], i8x16[6], i8x16[7], i8x16[7], i8x16[8], i8x16[8],
                 i8x16[9], i8x16[9], i8x16[10], i8x16[10], i8x16[11], i8x16[11],
                 i8x16[12], i8x16[12], i8x16[13], i8x16[13], i8x16[14],
                 i8x16[14], i8x16[15], i8x16[15]);
  }

 private:
  std::array<uint8_t, 16> val_ = {0};
};

inline std::ostream& operator<<(std::ostream& os, const Simd128& simd) {
  return os << absl::StrFormat("%v", simd);
}

}  // namespace v8::internal

#endif  // V8_COMMON_SIMD128_H_
