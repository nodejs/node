// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_WASM_WASM_ATOMICS_UTILS_H_
#define V8_UNITTESTS_WASM_WASM_ATOMICS_UTILS_H_

#include "test/common/value-helper.h"
#include "test/unittests/wasm/wasm-runner.h"

namespace v8 {
namespace internal {
namespace wasm {

#define WASM_ATOMIC_OPERATION_LIST(V) \
  V(Add)                              \
  V(Sub)                              \
  V(And)                              \
  V(Or)                               \
  V(Xor)                              \
  V(Exchange)

using Uint64BinOp = uint64_t (*)(uint64_t, uint64_t);
using Uint32BinOp = uint32_t (*)(uint32_t, uint32_t);
using Uint16BinOp = uint16_t (*)(uint16_t, uint16_t);
using Uint8BinOp = uint8_t (*)(uint8_t, uint8_t);

template <typename T>
T Add(T a, T b) {
  return a + b;
}

template <typename T>
T Sub(T a, T b) {
  return a - b;
}

template <typename T>
T And(T a, T b) {
  return a & b;
}

template <typename T>
T Or(T a, T b) {
  return a | b;
}

template <typename T>
T Xor(T a, T b) {
  return a ^ b;
}

template <typename T>
T Exchange(T a, T b) {
  return b;
}

template <typename T>
T CompareExchange(T initial, T a, T b) {
  if (initial == a) return b;
  return a;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_WASM_WASM_ATOMICS_UTILS_H_
