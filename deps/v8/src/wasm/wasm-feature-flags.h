// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_FEATURE_FLAGS_H_
#define V8_WASM_WASM_FEATURE_FLAGS_H_

// See https://github.com/WebAssembly/proposals for an overview of current
// WebAssembly proposals.

// Experimental features (disabled by default).
#define FOREACH_WASM_EXPERIMENTAL_FEATURE_FLAG(V) /*     (force 80 columns) */ \
  /* Exception handling proposal. */                                           \
  /* https://github.com/WebAssembly/exception-handling */                      \
  /* V8 side owner: clemensb */                                                \
  V(eh, "exception handling opcodes", false)                                   \
                                                                               \
  /* Fixed-width SIMD operations. */                                           \
  /* https://github.com/webassembly/simd */                                    \
  /* V8 side owner: gdeepti */                                                 \
  V(simd, "SIMD opcodes", false)                                               \
                                                                               \
  /* Tail call / return call proposal. */                                      \
  /* https://github.com/webassembly/tail-call */                               \
  /* V8 side owner: fgm */                                                     \
  V(return_call, "return call opcodes", false)                                 \
                                                                               \
  /* No official proposal (yet?). */                                           \
  /* V8 side owner: clemensb */                                                \
  V(compilation_hints, "compilation hints section", false)

// #############################################################################
// Staged features (disabled by default, but enabled via --wasm-staging (also
// exposed as chrome://flags/#enable-experimental-webassembly-features). Staged
// features get limited fuzzer coverage, and should come with their own tests.
// They are not run through all fuzzers though and don't get much exposure in
// the wild. Staged features do not necessarily be fully stabilized. They should
// be shipped with enough lead time to the next branch to allow for
// stabilization.
#define FOREACH_WASM_STAGING_FEATURE_FLAG(V) /*          (force 80 columns) */ \
  /* Reference Types, a.k.a. anyref proposal. */                               \
  /* https://github.com/WebAssembly/reference-types */                         \
  /* V8 side owner: ahaas */                                                   \
  /* Staged in v7.8. */                                                        \
  V(anyref, "anyref opcodes", false)                                           \
                                                                               \
  /* JS BitInt to wasm i64 integration. */                                     \
  /* https://github.com/WebAssembly/JS-BigInt-integration */                   \
  /* V8 side owner: ahaas, ssauleau@igalia.com */                              \
  /* Staged in v7.9. */                                                        \
  V(bigint, "JS BigInt support", false)                                        \
                                                                               \
  /* Multi-value proposal. */                                                  \
  /* https://github.com/WebAssembly/multi-value */                             \
  /* V8 side owner: thibaudm */                                                \
  /* Staged in v8.0. */                                                        \
  V(mv, "multi-value support", false)                                          \
                                                                               \
  /* Threads proposal. */                                                      \
  /* https://github.com/webassembly/threads */                                 \
  /* NOTE: This is enabled via chromium flag on desktop systems since v7.4  */ \
  /* (see https://crrev.com/c/1487808). ITS: https://groups.google.com/a/   */ \
  /* chromium.org/d/msg/blink-dev/tD6np-OG2PU/rcNGROOMFQAJ */                  \
  /* V8 side owner: gdeepti */                                                 \
  V(threads, "thread opcodes", false)                                          \
                                                                               \
  /* Type reflection proposal. */                                              \
  /* https://github.com/webassembly/js-types */                                \
  /* V8 side owner: ahaas */                                                   \
  /* Staged in v7.8. */                                                        \
  V(type_reflection, "wasm type reflection in JS", false)

// #############################################################################
// Shipped features (enabled by default). Remove the feature flag once they hit
// stable and are expected to stay enabled.
#define FOREACH_WASM_SHIPPED_FEATURE_FLAG(V) /*          (force 80 columns) */ \
  /* Bulk memory operations. */                                                \
  /* https://github.com/webassembly/bulk-memory-operations */                  \
  /* V8 side owner: binji */                                                   \
  /* Shipped in v7.5. */                                                       \
  /* ITS: https://groups.google.com/forum/#!topic/v8-users/zM05lYEBVog */      \
  V(bulk_memory, "bulk memory opcodes", true)

// Combination of all available wasm feature flags.
#define FOREACH_WASM_FEATURE_FLAG(V)        \
  FOREACH_WASM_EXPERIMENTAL_FEATURE_FLAG(V) \
  FOREACH_WASM_STAGING_FEATURE_FLAG(V)      \
  FOREACH_WASM_SHIPPED_FEATURE_FLAG(V)

#endif  // V8_WASM_WASM_FEATURE_FLAGS_H_
