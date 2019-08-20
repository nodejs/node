// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_FEATURE_FLAGS_H_
#define V8_WASM_WASM_FEATURE_FLAGS_H_

// The SEPARATOR argument allows generating proper comma-separated lists.
#define FOREACH_WASM_FEATURE_FLAG(V, SEPARATOR)                       \
  V(mv, "multi-value support", false)                                 \
  SEPARATOR                                                           \
  V(eh, "exception handling opcodes", false)                          \
  SEPARATOR                                                           \
  V(se, "sign extension opcodes", true)                               \
  SEPARATOR                                                           \
  V(sat_f2i_conversions, "saturating float conversion opcodes", true) \
  SEPARATOR                                                           \
  V(threads, "thread opcodes", false)                                 \
  SEPARATOR                                                           \
  V(simd, "SIMD opcodes", false)                                      \
  SEPARATOR                                                           \
  V(anyref, "anyref opcodes", false)                                  \
  SEPARATOR                                                           \
  V(bigint, "JS BigInt support", false)                               \
  SEPARATOR                                                           \
  V(bulk_memory, "bulk memory opcodes", true)                         \
  SEPARATOR                                                           \
  V(return_call, "return call opcodes", false)                        \
  SEPARATOR                                                           \
  V(type_reflection, "wasm type reflection in JS", false)             \
  SEPARATOR                                                           \
  V(compilation_hints, "compilation hints section", false)
#endif  // V8_WASM_WASM_FEATURE_FLAGS_H_
