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

namespace v8 {
namespace internal {
namespace compiler {

struct WasmTypeCheckConfig {
  bool object_can_be_null;
  bool null_succeeds;
  uint8_t rtt_depth;
};

V8_INLINE std::ostream& operator<<(std::ostream& os,
                                   WasmTypeCheckConfig const& p) {
  return os << (p.object_can_be_null ? "nullable" : "non-nullable")
            << ", depth=" << static_cast<int>(p.rtt_depth);
}

V8_INLINE size_t hash_value(WasmTypeCheckConfig const& p) {
  return base::hash_combine(p.object_can_be_null, p.rtt_depth);
}

V8_INLINE bool operator==(const WasmTypeCheckConfig& p1,
                          const WasmTypeCheckConfig& p2) {
  return p1.object_can_be_null == p2.object_can_be_null &&
         p1.rtt_depth == p2.rtt_depth;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_COMPILER_DEFINITIONS_H_
