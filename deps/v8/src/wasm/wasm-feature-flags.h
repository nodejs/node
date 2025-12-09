// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_FEATURE_FLAGS_H_
#define V8_WASM_WASM_FEATURE_FLAGS_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

// Each entry in this file generates a V8 command-line flag with the prefix
// "--experimental-wasm-".
//
// For example, to enable "my_feature", pass
// --experimental-wasm-my-feature to d8, or
// --js-flags=--experimental-wasm-my-feature to Chrome.
//
// To disable "my_feature", add the "--no-" prefix:
// --no-experimental-wasm-my-feature.
//
// See https://github.com/WebAssembly/proposals for an overview of current
// WebAssembly proposals.

// Experimental features (disabled by default).
#define FOREACH_WASM_EXPERIMENTAL_FEATURE_FLAG(V) /*     (force 80 columns) */ \
  /* Type reflection proposal. */                                              \
  /* https://github.com/webassembly/js-types */                                \
  /* V8 side owner: ahaas */                                                   \
  /* Staged in v7.8, unstaged in v13.6 (see https://crbug.com/402340845) */    \
  V(type_reflection, "wasm type reflection in JS", false)                      \
                                                                               \
  /* Instruction Tracing tool convention (early prototype, might change) */    \
  /* Tool convention: https://github.com/WebAssembly/tool-conventions */       \
  /* V8 side owner: jabraham */                                                \
  V(instruction_tracing, "instruction tracing section", false)                 \
                                                                               \
                                                                               \
  /* Shared-Everything Threads proposal. */                                    \
  /* https://github.com/WebAssembly/shared-everything-threads */               \
  /* V8 side owner: manoskouk */                                               \
  V(shared, "shared-everything threads", false)                                \
                                                                               \
  /* FP16 proposal. */                                                         \
  /* https://github.com/WebAssembly/half-precision */                          \
  /* V8 side owner: irezvov */                                                 \
  V(fp16, "fp16", false)                                                       \
                                                                               \
  /* Memory Control proposal */                                                \
  /* https://github.com/WebAssembly/memory-control */                          \
  /* V8 side owner: ahaas */                                                   \
  V(memory_control, "memory control", false)                                   \
                                                                               \
  /* Core stack switching, main proposal */                                    \
  /* https://github.com/WebAssembly/stack-switching */                         \
  /* V8 side owner: fgm */                                                     \
  V(wasmfx, "core stack switching", false)                                     \
                                                                               \
  /* Compilation hints */                                                      \
  /* https://github.com/WebAssembly/compilation-hints */                       \
  /* V8 side owner: ecmziegler, manoskouk */                                   \
  V(compilation_hints, "compilation hints", false)                             \
                                                                               \
  /* V8 side owner: thibaudm */                                                \
  V(growable_stacks, "growable stacks for jspi", false)

// #############################################################################
// Pre-staged features (disabled by default, but enabled via
// --experimental-fuzzing). Pre-staged features get limited fuzzer coverage, and
// should come with their own tests. Features typically spend about 2-4 weeks in
// this stage before being moved to the staging phase. It's therefore expected
// that this list is empty most of the time and that features spend extended
// time right before or after this phase.
#define FOREACH_WASM_PRE_STAGING_FEATURE_FLAG(V) /*      (force 80 columns) */ \
  /* Custom Descriptors proposal. */                                           \
  /* https://github.com/WebAssembly/custom-descriptors */                      \
  /* Note: the JS Interop part of the proposal is enabled by */                \
  /* --experimental-wasm-js-interop for now. */                                \
  /* V8 side owner: jkummerow */                                               \
  V(custom_descriptors, "custom descriptors", false)                           \
  // add pre-staged features right before this line

// #############################################################################
// Staged features (disabled by default, but enabled via --wasm-staging (also
// exposed as chrome://flags/#enable-experimental-webassembly-features). Staged
// features get limited fuzzer coverage, and should come with their own tests.
// They are not run through all fuzzers though and don't get much exposure in
// the wild. Staged features are not necessarily fully stabilized. They should
// be shipped with enough lead time to the next branch to allow for
// stabilization.
// Consider adding a chromium-side use counter if you want to track usage in the
// wild (also see {V8::UseCounterFeature}).
#define FOREACH_WASM_STAGING_FEATURE_FLAG(V) /*          (force 80 columns) */ \
  /* Reference-Typed Strings Proposal. */                                      \
  /* https://github.com/WebAssembly/stringref */                               \
  /* V8 side owner: jkummerow */                                               \
  V(stringref, "reference-typed strings", false)                               \
                                                                               \
  /* Imported Strings TextEncoder/TextDecoder post-MVP extension. */           \
  /* No upstream repo yet. */                                                  \
  /* V8 side owner: jkummerow */                                               \
  V(imported_strings_utf8, "imported strings (utf8 features)", false)

// #############################################################################
// Shipped features (enabled by default). Remove the feature flag once they hit
// stable and are expected to stay enabled.
#define FOREACH_WASM_SHIPPED_FEATURE_FLAG(V) /*          (force 80 columns) */ \
  /* Legacy exception handling proposal. */                                    \
  /* https://github.com/WebAssembly/exception-handling */                      \
  /* V8 side owner: thibaudm */                                                \
  /* Staged in v8.9 */                                                         \
  /* Shipped in v9.5 */                                                        \
  V(legacy_eh, "legacy exception handling opcodes", true)                      \
                                                                               \
  /* Branch Hinting proposal. */                                               \
  /* https://github.com/WebAssembly/branch-hinting */                          \
  /* V8 side owner: jkummerow */                                               \
  /* Staged in v13.6. */                                                       \
  /* Shipped in v13.7. */                                                      \
  V(branch_hinting, "branch hinting", true)                                    \
                                                                               \
  /* Exnref */                                                                 \
  /* This flag enables the new exception handling proposal */                  \
  /* V8 side owner: thibaudm */                                                \
  /* Shipped in v13.7 */                                                       \
  V(exnref, "exnref", true)                                                    \
                                                                               \
  /* Resizable buffer integration */                                           \
  /* https://github.com/WebAssembly/spec/issues/1292 */                        \
  /* V8 side owner: gdeepti */                                                 \
  /* Staged in v14.4 */                                                        \
  V(rab_integration, "resizable buffers integration", true)

// Combination of all available wasm feature flags.
#define FOREACH_WASM_FEATURE_FLAG(V)        \
  FOREACH_WASM_EXPERIMENTAL_FEATURE_FLAG(V) \
  FOREACH_WASM_PRE_STAGING_FEATURE_FLAG(V)  \
  FOREACH_WASM_STAGING_FEATURE_FLAG(V)      \
  FOREACH_WASM_SHIPPED_FEATURE_FLAG(V)

// Consistency check: Experimental and staged features are off by default.
#define CHECK_WASM_FEATURE_OFF_BY_DEFAULT(name, desc, enabled) \
  static_assert(enabled == false);
#define CHECK_WASM_FEATURE_ON_BY_DEFAULT(name, desc, enabled) \
  static_assert(enabled == true);
FOREACH_WASM_EXPERIMENTAL_FEATURE_FLAG(CHECK_WASM_FEATURE_OFF_BY_DEFAULT)
FOREACH_WASM_PRE_STAGING_FEATURE_FLAG(CHECK_WASM_FEATURE_OFF_BY_DEFAULT)
FOREACH_WASM_STAGING_FEATURE_FLAG(CHECK_WASM_FEATURE_OFF_BY_DEFAULT)
FOREACH_WASM_SHIPPED_FEATURE_FLAG(CHECK_WASM_FEATURE_ON_BY_DEFAULT)
#undef CHECK_WASM_FEATURE_OFF_BY_DEFAULT
#undef CHECK_WASM_FEATURE_ON_BY_DEFAULT

#endif  // V8_WASM_WASM_FEATURE_FLAGS_H_
