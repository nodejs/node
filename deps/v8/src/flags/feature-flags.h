// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FLAGS_FEATURE_FLAGS_H_
#define V8_FLAGS_FEATURE_FLAGS_H_

// Any V8 feature or large scale change that follows the launch process should
// be tracked here for transparency. This ensures that the requirements for
// each step of the process are met, e.g. enabling it for fuzzers, opening it
// to the VRP, etc.

// For each step of the shipping process, there is a dedicated section below
// with a macro. The entries for each feature move from one macro to the next
// as they go through the shipping process.

// Each flag is defined by the following parameters:
//   1. Type - either JS_FEATURE, WASM_FEATURE, or INTERNAL_FEATURE depending on
//      whether the feature is a TC39 proposal, a Wasm CG proposal or something
//      else (e.g. performance or internal feature).
//   2. Name - the name of the feature which will be used as the flag name
//      WASM_FEATUREs will be prefixed with wasm_ in their flag name.
//   3. Description - this serves as the help text for the feature flag.

// Additionally, the feature should be annotated with a comment containing
//   1. Its internal name (might be the same as above or different)
//   2. A link to the proposal where applicable
//   3. V8-side owner
//   4. Information on Finch or origin trials, their status and links

// Update bootstrapper.cc whenever adding a new JS/harmony feature flag.
// The "harmony" naming is now outdated and will no longer be used for new JS
// features.
// TODO(v8:14214): Remove --harmony flags once transition is complete.

// Features that should only be present when certain build flags are set need to
// define their own conditional macro for that feature.
#ifdef V8_INTL_SUPPORT
#define IF_INTL_ENABLED(then, feature_name, description) \
  then(feature_name, description)
#else
#define IF_INTL_ENABLED(then, feature_name, description)
#endif

#ifdef V8_ENABLE_WASM_SIMD256_REVEC
#define IF_REVEC_ENABLED(then, feature_name, description) \
  then(feature_name, description)
#else
#define IF_REVEC_ENABLED(then, feature_name, description)
#endif  // V8_ENABLE_WASM_SIMD256_REVEC

#ifdef V8_ENABLE_SPARKPLUG_PLUS
#define IF_SPARKPLUG_PLUS_ENABLED(then, feature_name, description) \
  then(feature_name, description)
#else
#define IF_SPARKPLUG_PLUS_ENABLED(then, feature_name, description)
// If the build flag is not defined, we add a read-only flag in
// flag-definitions.h instead.
#endif  // V8_ENABLE_SPARKPLUG_PLUS

// #############################################################################
// Experimental features (disabled by default).
// These features are still in active development and not stable enough for
// fuzzing or developer testing yet.
#define FOREACH_EXPERIMENTAL_FEATURE_FLAG(JS_FEATURE, WASM_FEATURE,            \
                                          INTERNAL_FEATURE) /* (80 columns) */ \
                                                                               \
  JS_FEATURE(harmony_shadow_realm, "harmony ShadowRealm")                      \
                                                                               \
  JS_FEATURE(harmony_struct,                                                   \
             "harmony structs, shared structs, and shared arrays")             \
                                                                               \
  IF_INTL_ENABLED(JS_FEATURE, harmony_intl_best_fit_matcher,                   \
                  "Intl BestFitMatcher")                                       \
                                                                               \
  JS_FEATURE(js_decorators, "decorators")                                      \
                                                                               \
  JS_FEATURE(js_source_phase_imports, "source phase imports")                  \
                                                                               \
  JS_FEATURE(js_regexp_buffer_boundaries,                                      \
             "RegExp \\A \\z \\Z buffer boundary assertions")                  \
                                                                               \
  IF_SPARKPLUG_PLUS_ENABLED(INTERNAL_FEATURE, sparkplug_inline_smi,            \
                            "inline the Smi fast path of embedded feedback "   \
                            "operations into JS baseline code")                \
                                                                               \
  /* Instruction Tracing tool convention (early prototype, might change) */    \
  /* Tool convention: https://github.com/WebAssembly/tool-conventions */       \
  /* V8 side owner: jabraham */                                                \
  WASM_FEATURE(instruction_tracing, "instruction tracing section")             \
                                                                               \
  /* Shared-Everything Threads proposal. */                                    \
  /* https://github.com/WebAssembly/shared-everything-threads */               \
  /* V8 side owner: manoskouk */                                               \
  WASM_FEATURE(shared, "shared-everything threads")                            \
                                                                               \
  /* FP16 proposal. */                                                         \
  /* https://github.com/WebAssembly/half-precision */                          \
  /* V8 side owner: dahlb */                                                   \
  WASM_FEATURE(fp16, "fp16")                                                   \
                                                                               \
  /* Memory Control proposal */                                                \
  /* https://github.com/WebAssembly/memory-control */                          \
  /* V8 side owner: ahaas */                                                   \
  WASM_FEATURE(memory_control, "memory control")                               \
                                                                               \
  /* Core stack switching, main proposal */                                    \
  /* https://github.com/WebAssembly/stack-switching */                         \
  /* V8 side owner: fgm, thibaudm */                                           \
  WASM_FEATURE(wasmfx, "core stack switching")                                 \
                                                                               \
  /* Compilation hints */                                                      \
  /* https://github.com/WebAssembly/compilation-hints */                       \
  /* V8 side owner: ecmziegler, manoskouk */                                   \
  WASM_FEATURE(compilation_hints, "compilation hints")                         \
                                                                               \
  /* V8 side owner: thibaudm */                                                \
  WASM_FEATURE(growable_stacks, "growable stacks for jspi")                    \
                                                                               \
  /* Compact Import Section proposal. */                                       \
  /* https://github.com/WebAssembly/compact-import-section */                  \
  /* V8 side owner: ryandiaz */                                                \
  WASM_FEATURE(compact_imports, "compact import section")

// #############################################################################
// Pre-staged features (disabled by default, but enabled via
// --experimental-fuzzing).
// Pre-staged features get limited fuzzer coverage, and should come with their
// own tests. Features typically spend about 2-4 weeks in this stage before
// being moved to the staging phase.
#define FOREACH_PRE_STAGED_FEATURE_FLAG(JS_FEATURE, WASM_FEATURE,              \
                                        INTERNAL_FEATURE) /*   (80 columns) */ \
                                                                               \
  /* Reference-Typed Strings Proposal. */                                      \
  /* https://github.com/WebAssembly/stringref */                               \
  /* V8 side owner: jkummerow */                                               \
  WASM_FEATURE(stringref, "reference-typed strings")                           \
                                                                               \
  /* Imported Strings TextEncoder/TextDecoder post-MVP extension. */           \
  /* No upstream repo yet. */                                                  \
  /* V8 side owner: jkummerow */                                               \
  WASM_FEATURE(imported_strings_utf8, "imported strings (utf8 features)")      \
                                                                               \
  /* Wide Arithmetic proposal */                                               \
  /* https://github.com/WebAssembly/wide-arithmetic */                         \
  /* V8 side owner: ryandiaz */                                                \
  WASM_FEATURE(wide_arithmetic, "wide arithmetic")                             \
                                                                               \
  /* Acquire-Release memory ordering from Shared-Everything Threads */         \
  /* proposal. */                                                              \
  /* Part of https://github.com/WebAssembly/shared-everything-threads */       \
  /* V8 side owner: rezvan */                                                  \
  WASM_FEATURE(acquire_release, "acquire_release memory ordering")

// #############################################################################
// Staged features (disabled by default, but enabled via --js-staging/--harmony,
// --wasm-staging or --future).
// Staged features get limited fuzzer coverage, and should come with their own
// tests.
// They might be enabled for an origin or Finch trial after some bake time
// while remaining in this stage before moving to shipped.
// They are not run through all fuzzers though and don't get much exposure in
// the wild. Staged features are not necessarily fully stabilized. They should
// be shipped with enough lead time to the next branch to allow for
// stabilization.
// Consider adding a chromium-side use counter if you want to track usage in
// the wild (also see {V8::UseCounterFeature}).
#define FOREACH_STAGED_FEATURE_FLAG(JS_FEATURE, WASM_FEATURE,                  \
                                    INTERNAL_FEATURE) /*       (80 columns) */ \
                                                                               \
  JS_FEATURE(js_immutable_arraybuffer, "Immutable ArrayBuffer")                \
                                                                               \
  JS_FEATURE(js_import_text, "import text")                                    \
                                                                               \
  JS_FEATURE(js_import_bytes, "import bytes")                                  \
                                                                               \
  JS_FEATURE(js_defer_import_eval, "defer import eval")                        \
                                                                               \
  /* Custom Descriptors proposal. */                                           \
  /* https://github.com/WebAssembly/custom-descriptors */                      \
  /* Note: the JS Interop part of the proposal is enabled by */                \
  /* --wasm-js-interop for now. */                                             \
  /* V8 side owner: jkummerow */                                               \
  /* Staged (without JS Interop) in v14.8 */                                   \
  WASM_FEATURE(custom_descriptors, "custom descriptors")                       \
                                                                               \
  /* Wasm Re-vectorization (no proposal, engine optimization only). */         \
  /* V8 side owner: jkummerow */                                               \
  /* Staged in 15.4 */                                                         \
  IF_REVEC_ENABLED(INTERNAL_FEATURE, wasm_revectorize,                         \
                   "128 to 256 bit SIMD re-vectorization for Wasm")

// #############################################################################
// Shipped features (enabled by default).
// The feature flag can be removed after it has been shipped on stable for a
// sufficiently long time that an emergency deactivation option is not required
// anymore. In some cases, features might stay on this list for longer if the
// ability to deactivate them is still useful.
#define FOREACH_SHIPPED_FEATURE_FLAG(JS_FEATURE, WASM_FEATURE,                 \
                                     INTERNAL_FEATURE) /*      (80 columns) */ \
                                                                               \
  JS_FEATURE(harmony_import_attributes, "harmony import attributes")           \
                                                                               \
  JS_FEATURE(harmony_temporal, "Temporal")                                     \
                                                                               \
  JS_FEATURE(js_esm_ns_reexport,                                               \
             "Support diamond-importing re-expored namespaces "                \
             "(https://github.com/tc39/ecma262/pull/3715)")                    \
                                                                               \
  JS_FEATURE(js_iterator_join, "Iterator.prototype.join")                      \
                                                                               \
  JS_FEATURE(js_iterator_sequencing, "iterator sequencing")                    \
                                                                               \
  JS_FEATURE(js_joint_iteration, "joint iteration")                            \
                                                                               \
  JS_FEATURE(js_pr_3883,                                                       \
             "Promise.try not wrapping the result in an extra promise in the " \
             "non-throwing case (https://github.com/tc39/ecma262/pull/3883)")  \
                                                                               \
  JS_FEATURE(js_iterator_includes, "Iterator.prototype.includes")              \
                                                                               \
  /* Legacy exception handling proposal. */                                    \
  /* https://github.com/WebAssembly/exception-handling */                      \
  /* V8 side owner: thibaudm */                                                \
  /* Staged in v8.9 */                                                         \
  /* Shipped in v9.5 */                                                        \
  WASM_FEATURE(legacy_eh, "legacy exception handling opcodes")                 \
                                                                               \
  IF_SPARKPLUG_PLUS_ENABLED(INTERNAL_FEATURE, sparkplug_plus,                  \
                            "dynamic patching on JS baseline code")

#endif  // V8_FLAGS_FEATURE_FLAGS_H_
