// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_VALUE_H_
#define V8_WASM_WASM_VALUE_H_

#include "src/boxed-float.h"
#include "src/v8memory.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace wasm {

#define FOREACH_SIMD_TYPE(V) \
  V(float, float4, f32x4, 4) \
  V(int32_t, int4, i32x4, 4) \
  V(int16_t, int8, i16x8, 8) \
  V(int8_t, int16, i8x16, 16)

#define DEFINE_SIMD_TYPE(cType, sType, name, kSize) \
  struct sType {                                    \
    cType val[kSize];                               \
  };
FOREACH_SIMD_TYPE(DEFINE_SIMD_TYPE)
#undef DEFINE_SIMD_TYPE

class Simd128 {
 public:
  Simd128() : val_() {
    for (size_t i = 0; i < 16; i++) {
      val_[i] = 0;
    }
  }
#define DEFINE_SIMD_TYPE_SPECIFIC_METHODS(cType, sType, name, size)    \
  explicit Simd128(sType val) {                                        \
    WriteUnalignedValue<sType>(reinterpret_cast<Address>(val_), val);  \
  }                                                                    \
  sType to_##name() {                                                  \
    return ReadUnalignedValue<sType>(reinterpret_cast<Address>(val_)); \
  }
  FOREACH_SIMD_TYPE(DEFINE_SIMD_TYPE_SPECIFIC_METHODS)
#undef DEFINE_SIMD_TYPE_SPECIFIC_METHODS

 private:
  uint8_t val_[16];
};

// Macro for defining WasmValue methods for different types.
// Elements:
// - name (for to_<name>() method)
// - wasm type
// - c type
#define FOREACH_WASMVAL_TYPE(V)   \
  V(i32, kWasmI32, int32_t)       \
  V(u32, kWasmI32, uint32_t)      \
  V(i64, kWasmI64, int64_t)       \
  V(u64, kWasmI64, uint64_t)      \
  V(f32, kWasmF32, float)         \
  V(f32_boxed, kWasmF32, Float32) \
  V(f64, kWasmF64, double)        \
  V(f64_boxed, kWasmF64, Float64) \
  V(s128, kWasmS128, Simd128)

// A wasm value with type information.
class WasmValue {
 public:
  WasmValue() : type_(kWasmStmt), bit_pattern_{} {}

#define DEFINE_TYPE_SPECIFIC_METHODS(name, localtype, ctype)                   \
  explicit WasmValue(ctype v) : type_(localtype), bit_pattern_{} {             \
    static_assert(sizeof(ctype) <= sizeof(bit_pattern_),                       \
                  "size too big for WasmValue");                               \
    WriteUnalignedValue<ctype>(reinterpret_cast<Address>(bit_pattern_), v);    \
  }                                                                            \
  ctype to_##name() const {                                                    \
    DCHECK_EQ(localtype, type_);                                               \
    return to_##name##_unchecked();                                            \
  }                                                                            \
  ctype to_##name##_unchecked() const {                                        \
    return ReadUnalignedValue<ctype>(reinterpret_cast<Address>(bit_pattern_)); \
  }
  FOREACH_WASMVAL_TYPE(DEFINE_TYPE_SPECIFIC_METHODS)
#undef DEFINE_TYPE_SPECIFIC_METHODS

  ValueType type() const { return type_; }

  // Checks equality of type and bit pattern (also for float and double values).
  bool operator==(const WasmValue& other) const {
    return type_ == other.type_ &&
           !memcmp(bit_pattern_, other.bit_pattern_, 16);
  }

  template <typename T>
  inline T to() const;

  template <typename T>
  inline T to_unchecked() const;

 private:
  ValueType type_;
  uint8_t bit_pattern_[16];
};

#define DECLARE_CAST(name, localtype, ctype, ...) \
  template <>                                     \
  inline ctype WasmValue::to_unchecked() const {  \
    return to_##name##_unchecked();               \
  }                                               \
  template <>                                     \
  inline ctype WasmValue::to() const {            \
    return to_##name();                           \
  }
FOREACH_WASMVAL_TYPE(DECLARE_CAST)
#undef DECLARE_CAST

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_VALUE_H_
