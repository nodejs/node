// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_WASM_COMPILER_DEFINITIONS_H_
#define V8_COMPILER_WASM_COMPILER_DEFINITIONS_H_

#include <cstdint>
#include <ostream>

#include "src/base/functional.h"
#include "src/wasm/value-type.h"

namespace v8 {
namespace internal {
namespace compiler {

// If {to} is nullable, it means that null passes the check.
// {from} may change in compiler optimization passes as the object's type gets
// narrowed.
// TODO(12166): Add modules if we have cross-module inlining.
struct WasmTypeCheckConfig {
  wasm::ValueType from;
  const wasm::ValueType to;
};

V8_INLINE std::ostream& operator<<(std::ostream& os,
                                   WasmTypeCheckConfig const& p) {
  return os << p.from.name() << " -> " << p.to.name();
}

V8_INLINE size_t hash_value(WasmTypeCheckConfig const& p) {
  return base::hash_combine(p.from.raw_bit_field(), p.to.raw_bit_field());
}

V8_INLINE bool operator==(const WasmTypeCheckConfig& p1,
                          const WasmTypeCheckConfig& p2) {
  return p1.from == p2.from && p1.to == p2.to;
}

static constexpr int kCharWidthBailoutSentinel = 3;

enum NullCheckStrategy { kExplicitNullChecks, kTrapHandler };

// Static knowledge about whether a wasm-gc operation, such as struct.get, needs
// a null check.
enum CheckForNull { kWithoutNullCheck, kWithNullCheck };

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_COMPILER_DEFINITIONS_H_
