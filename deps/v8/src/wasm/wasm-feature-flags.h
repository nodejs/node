// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_WASM_FEATURE_FLAGS_H_
#define V8_WASM_WASM_FEATURE_FLAGS_H_

// See https://github.com/WebAssembly/proposals for an overview of current
// WebAssembly proposals.

// Experimental features (disabled by default).
#define FOREACH_WASM_EXPERIMENTAL_FEATURE_FLAG(V) /*     (force 80 columns) */ \
  /* No official proposal (yet?). */                                           \
  /* V8 side owner: clemensb */                                                \
  V(compilation_hints, "compilation hints section", false)                     \
                                                                               \
  /* GC proposal (early prototype, might change dramatically) */               \
  /* Official proposal: https://github.com/WebAssembly/gc */                   \
  /* Prototype engineering spec: https://bit.ly/3cWcm6Q */                     \
  /* V8 side owner: jkummerow */                                               \
  V(gc, "garbage collection", false)                                           \
                                                                               \
  /* Typed function references proposal. */                                    \
  /* Official proposal: https://github.com/WebAssembly/function-references */  \
  /* V8 side owner: manoskouk */                                               \
  V(typed_funcref, "typed function references", false)                         \
                                                                               \
  /* Memory64 proposal. */                                                     \
  /* https://github.com/WebAssembly/memory64 */                                \
  /* V8 side owner: clemensb */                                                \
  V(memory64, "memory64", false)                                               \
                                                                               \
  /* Relaxed SIMD proposal. */                                                 \
  /* https://github.com/WebAssembly/relaxed-simd */                            \
  /* V8 side owner: zhin */                                                    \
  V(relaxed_simd, "relaxed simd", false)

// #############################################################################
// Staged features (disabled by default, but enabled via --wasm-staging (also
// exposed as chrome://flags/#enable-experimental-webassembly-features). Staged
// features get limited fuzzer coverage, and should come with their own tests.
// They are not run through all fuzzers though and don't get much exposure in
// the wild. Staged features do not necessarily be fully stabilized. They should
// be shipped with enough lead time to the next branch to allow for
// stabilization.
#define FOREACH_WASM_STAGING_FEATURE_FLAG(V) /*          (force 80 columns) */ \
  /* Exception handling proposal. */                                           \
  /* https://github.com/WebAssembly/exception-handling */                      \
  /* V8 side owner: thibaudm */                                                \
  /* Staged in v8.9 */                                                         \
  V(eh, "exception handling opcodes", false)                                   \
                                                                               \
  /* Reference Types, a.k.a. reftypes proposal. */                             \
  /* https://github.com/WebAssembly/reference-types */                         \
  /* V8 side owner: ahaas */                                                   \
  /* Staged in v7.8. */                                                        \
  V(reftypes, "reference type opcodes", false)                                 \
                                                                               \
  /* Tail call / return call proposal. */                                      \
  /* https://github.com/webassembly/tail-call */                               \
  /* V8 side owner: thibaudm */                                                \
  /* Staged in v8.7 * */                                                       \
  V(return_call, "return call opcodes", false)                                 \
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
  /* Multi-value proposal. */                                                  \
  /* https://github.com/WebAssembly/multi-value */                             \
  /* V8 side owner: thibaudm */                                                \
  /* Shipped in v8.6. */                                                       \
  /* ITS: https://groups.google.com/g/v8-users/c/pv2E4yFWeF0 */                \
  V(mv, "multi-value support", true)                                           \
                                                                               \
  /* Fixed-width SIMD operations. */                                           \
  /* https://github.com/webassembly/simd */                                    \
  /* V8 side owner: gdeepti, zhin */                                           \
  /* Staged in v8.7 * */                                                       \
  /* Shipped in v9.1 * */                                                      \
  V(simd, "SIMD opcodes", true)                                                \
                                                                               \
  /* Threads proposal. */                                                      \
  /* https://github.com/webassembly/threads */                                 \
  /* NOTE: This is enabled via chromium flag on desktop systems since v7.4, */ \
  /* and on android from 9.1. Threads are only available when */               \
  /* SharedArrayBuffers are enabled as well, and are gated by COOP/COEP */     \
  /* headers, more fine grained control is in the chromium codebase */         \
  /* ITS: https://groups.google.com/a/chromium.org/d/msg/blink-dev/ */         \
  /* tD6np-OG2PU/rcNGROOMFQAJ */                                               \
  /* V8 side owner: gdeepti */                                                 \
  V(threads, "thread opcodes", true)                                           \
                                                                               \
// Combination of all available wasm feature flags.
#define FOREACH_WASM_FEATURE_FLAG(V)        \
  FOREACH_WASM_EXPERIMENTAL_FEATURE_FLAG(V) \
  FOREACH_WASM_STAGING_FEATURE_FLAG(V)      \
  FOREACH_WASM_SHIPPED_FEATURE_FLAG(V)

#endif  // V8_WASM_WASM_FEATURE_FLAGS_H_
