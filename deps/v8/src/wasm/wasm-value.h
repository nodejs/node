// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_VALUE_H_
#define V8_WASM_VALUE_H_

#include "src/boxed-float.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace wasm {

// Macro for defining WasmValue methods for different types.
// Elements:
// - name (for to_<name>() method)
// - wasm type
// - c type
// - how to get bit pattern from value {v} of type {c type}
// - how to get value of type {c type} from bit pattern {p}
#define FOREACH_WASMVAL_TYPE(V)                                                \
  V(i32, kWasmI32, int32_t, static_cast<uint32_t>(v), static_cast<int32_t>(p)) \
  V(u32, kWasmI32, uint32_t, v, static_cast<uint32_t>(p))                      \
  V(i64, kWasmI64, int64_t, static_cast<uint64_t>(v), static_cast<int64_t>(p)) \
  V(u64, kWasmI64, uint64_t, v, p)                                             \
  V(f32, kWasmF32, float, bit_cast<uint32_t>(v),                               \
    bit_cast<float>(static_cast<uint32_t>(p)))                                 \
  V(f32_boxed, kWasmF32, Float32, v.get_bits(),                                \
    Float32::FromBits(static_cast<uint32_t>(p)))                               \
  V(f64, kWasmF64, double, bit_cast<uint64_t>(v), bit_cast<double>(p))         \
  V(f64_boxed, kWasmF64, Float64, v.get_bits(), Float64::FromBits(p))

// A wasm value with type information.
class WasmValue {
 public:
  WasmValue() : type_(kWasmStmt) {}

#define DEFINE_TYPE_SPECIFIC_METHODS(name, localtype, ctype, v_to_p, p_to_v) \
  explicit WasmValue(ctype v) : type_(localtype), bit_pattern_(v_to_p) {}    \
  ctype to_##name() const {                                                  \
    DCHECK_EQ(localtype, type_);                                             \
    return to_##name##_unchecked();                                          \
  }                                                                          \
  ctype to_##name##_unchecked() const {                                      \
    auto p = bit_pattern_;                                                   \
    return p_to_v;                                                           \
  }
  FOREACH_WASMVAL_TYPE(DEFINE_TYPE_SPECIFIC_METHODS)
#undef DEFINE_TYPE_SPECIFIC_METHODS

  ValueType type() const { return type_; }

  // Checks equality of type and bit pattern (also for float and double values).
  bool operator==(const WasmValue& other) const {
    return type_ == other.type_ && bit_pattern_ == other.bit_pattern_;
  }

  template <typename T>
  inline T to() const;

  template <typename T>
  inline T to_unchecked() const;

 private:
  ValueType type_;
  uint64_t bit_pattern_;
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

#endif  // V8_WASM_VALUE_H_
