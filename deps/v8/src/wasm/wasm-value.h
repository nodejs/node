// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_VALUE_H_
#define V8_WASM_VALUE_H_

#include "src/wasm/wasm-opcodes.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace wasm {

// Macro for defining WasmValue union members.
#define FOREACH_WASMVAL_UNION_MEMBER(V) \
  V(i32, kWasmI32, int32_t)             \
  V(u32, kWasmI32, uint32_t)            \
  V(i64, kWasmI64, int64_t)             \
  V(u64, kWasmI64, uint64_t)            \
  V(f32, kWasmF32, float)               \
  V(f64, kWasmF64, double)

// A wasm value with type information.
class WasmValue {
 public:
  WasmValue() : type_(kWasmStmt) {}

#define DEFINE_TYPE_SPECIFIC_METHODS(field, localtype, ctype)          \
  explicit WasmValue(ctype v) : type_(localtype) { value_.field = v; } \
  ctype to_##field() const {                                           \
    DCHECK_EQ(localtype, type_);                                       \
    return value_.field;                                               \
  }
  FOREACH_WASMVAL_UNION_MEMBER(DEFINE_TYPE_SPECIFIC_METHODS)
#undef DEFINE_TYPE_SPECIFIC_METHODS

  ValueType type() const { return type_; }

  bool operator==(const WasmValue& other) const {
    if (type_ != other.type_) return false;
#define CHECK_VALUE_EQ(field, localtype, ctype) \
  if (type_ == localtype) {                     \
    return value_.field == other.value_.field;  \
  }
    FOREACH_WASMVAL_UNION_MEMBER(CHECK_VALUE_EQ)
#undef CHECK_VALUE_EQ
    UNREACHABLE();
  }

  template <typename T>
  inline T to() const {
    static_assert(sizeof(T) == -1, "Do only use this method with valid types");
  }

  template <typename T>
  inline T to_unchecked() const {
    static_assert(sizeof(T) == -1, "Do only use this method with valid types");
  }

 private:
  ValueType type_;
  union {
#define DECLARE_FIELD(field, localtype, ctype) ctype field;
    FOREACH_WASMVAL_UNION_MEMBER(DECLARE_FIELD)
#undef DECLARE_FIELD
  } value_;
};

#define DECLARE_CAST(field, localtype, ctype)    \
  template <>                                    \
  inline ctype WasmValue::to_unchecked() const { \
    return value_.field;                         \
  }                                              \
  template <>                                    \
  inline ctype WasmValue::to() const {           \
    return to_##field();                         \
  }
FOREACH_WASMVAL_UNION_MEMBER(DECLARE_CAST)
#undef DECLARE_CAST

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_VALUE_H_
