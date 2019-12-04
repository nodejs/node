// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_FEATURE_FLAGS_H_
#define V8_WASM_WASM_FEATURE_FLAGS_H_

#define FOREACH_WASM_EXPERIMENTAL_FEATURE_FLAG(V) \
  V(mv, "multi-value support", false)             \
  V(eh, "exception handling opcodes", false)      \
  V(threads, "thread opcodes", false)             \
  V(simd, "SIMD opcodes", false)                  \
  V(bigint, "JS BigInt support", false)           \
  V(return_call, "return call opcodes", false)    \
  V(compilation_hints, "compilation hints section", false)

#define FOREACH_WASM_STAGING_FEATURE_FLAG(V) \
  V(anyref, "anyref opcodes", false)         \
  V(type_reflection, "wasm type reflection in JS", false)

#define FOREACH_WASM_SHIPPED_FEATURE_FLAG(V)                          \
  V(bulk_memory, "bulk memory opcodes", true)                         \
  V(sat_f2i_conversions, "saturating float conversion opcodes", true) \
  V(se, "sign extension opcodes", true)

#define FOREACH_WASM_FEATURE_FLAG(V)        \
  FOREACH_WASM_EXPERIMENTAL_FEATURE_FLAG(V) \
  FOREACH_WASM_STAGING_FEATURE_FLAG(V)      \
  FOREACH_WASM_SHIPPED_FEATURE_FLAG(V)

#endif  // V8_WASM_WASM_FEATURE_FLAGS_H_
