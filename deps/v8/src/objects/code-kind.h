// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CODE_KIND_H_
#define V8_OBJECTS_CODE_KIND_H_

#include "src/base/flags.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {

// The order of INTERPRETED_FUNCTION to TURBOFAN is important. We use it to
// check the relative ordering of the tiers when fetching / installing optimized
// code.
#define CODE_KIND_LIST(V)       \
  V(BYTECODE_HANDLER)           \
  V(FOR_TESTING)                \
  V(BUILTIN)                    \
  V(REGEXP)                     \
  V(WASM_FUNCTION)              \
  V(WASM_TO_CAPI_FUNCTION)      \
  V(WASM_TO_JS_FUNCTION)        \
  V(JS_TO_WASM_FUNCTION)        \
  V(JS_TO_JS_FUNCTION)          \
  V(C_WASM_ENTRY)               \
  V(INTERPRETED_FUNCTION)       \
  V(NATIVE_CONTEXT_INDEPENDENT) \
  V(TURBOPROP)                  \
  V(TURBOFAN)

enum class CodeKind {
#define DEFINE_CODE_KIND_ENUM(name) name,
  CODE_KIND_LIST(DEFINE_CODE_KIND_ENUM)
#undef DEFINE_CODE_KIND_ENUM
};
STATIC_ASSERT(CodeKind::INTERPRETED_FUNCTION < CodeKind::TURBOPROP &&
              CodeKind::INTERPRETED_FUNCTION <
                  CodeKind::NATIVE_CONTEXT_INDEPENDENT);
STATIC_ASSERT(CodeKind::TURBOPROP < CodeKind::TURBOFAN &&
              CodeKind::NATIVE_CONTEXT_INDEPENDENT < CodeKind::TURBOFAN);

#define V(...) +1
static constexpr int kCodeKindCount = CODE_KIND_LIST(V);
#undef V

const char* CodeKindToString(CodeKind kind);

const char* CodeKindToMarker(CodeKind kind);

inline constexpr bool CodeKindIsInterpretedJSFunction(CodeKind kind) {
  return kind == CodeKind::INTERPRETED_FUNCTION;
}

inline constexpr bool CodeKindIsNativeContextIndependentJSFunction(
    CodeKind kind) {
  return kind == CodeKind::NATIVE_CONTEXT_INDEPENDENT;
}

inline constexpr bool CodeKindIsOptimizedJSFunction(CodeKind kind) {
  return kind == CodeKind::TURBOFAN ||
         kind == CodeKind::NATIVE_CONTEXT_INDEPENDENT ||
         kind == CodeKind::TURBOPROP;
}

inline constexpr bool CodeKindIsJSFunction(CodeKind kind) {
  return kind == CodeKind::INTERPRETED_FUNCTION ||
         CodeKindIsOptimizedJSFunction(kind);
}

inline constexpr bool CodeKindIsBuiltinOrJSFunction(CodeKind kind) {
  return kind == CodeKind::BUILTIN || CodeKindIsJSFunction(kind);
}

inline constexpr bool CodeKindCanDeoptimize(CodeKind kind) {
  // Even though NCI code does not deopt by itself at the time of writing,
  // tests may trigger deopts manually and thus we cannot make a narrower
  // distinction here.
  return CodeKindIsOptimizedJSFunction(kind);
}

inline constexpr bool CodeKindCanOSR(CodeKind kind) {
  return kind == CodeKind::TURBOFAN || kind == CodeKind::TURBOPROP;
}

inline constexpr bool CodeKindIsOptimizedAndCanTierUp(CodeKind kind) {
  return kind == CodeKind::NATIVE_CONTEXT_INDEPENDENT ||
         (FLAG_turboprop_as_midtier && kind == CodeKind::TURBOPROP);
}

inline constexpr bool CodeKindCanTierUp(CodeKind kind) {
  return kind == CodeKind::INTERPRETED_FUNCTION ||
         CodeKindIsOptimizedAndCanTierUp(kind);
}

// The optimization marker field on the feedback vector has a dual purpose of
// controlling the tier-up workflow, and caching the produced code object for
// access from multiple closures. The marker is not used for all code kinds
// though, in particular it is not used when generating NCI code.
inline constexpr bool CodeKindIsStoredInOptimizedCodeCache(CodeKind kind) {
  return kind == CodeKind::TURBOFAN || kind == CodeKind::TURBOPROP;
}

inline OptimizationTier GetTierForCodeKind(CodeKind kind) {
  if (kind == CodeKind::TURBOFAN) return OptimizationTier::kTopTier;
  if (kind == CodeKind::TURBOPROP) {
    return FLAG_turboprop_as_midtier ? OptimizationTier::kMidTier
                                     : OptimizationTier::kTopTier;
  }
  if (kind == CodeKind::NATIVE_CONTEXT_INDEPENDENT) {
    return FLAG_turbo_nci_as_midtier ? OptimizationTier::kMidTier
                                     : OptimizationTier::kTopTier;
  }
  return OptimizationTier::kNone;
}

inline CodeKind CodeKindForTopTier() {
  // TODO(turboprop, mythria): We should make FLAG_turboprop mean turboprop is
  // mid-tier compiler and replace FLAG_turboprop_as_midtier with
  // FLAG_turboprop_as_top_tier to tier up to only Turboprop once
  // FLAG_turboprop_as_midtier is stable and major regressions are addressed.
  if (V8_UNLIKELY(FLAG_turboprop)) {
    return FLAG_turboprop_as_midtier ? CodeKind::TURBOFAN : CodeKind::TURBOPROP;
  }
  return CodeKind::TURBOFAN;
}

// The dedicated CodeKindFlag enum represents all code kinds in a format
// suitable for bit sets.
enum class CodeKindFlag {
#define V(name) name = 1 << static_cast<int>(CodeKind::name),
  CODE_KIND_LIST(V)
#undef V
};
STATIC_ASSERT(kCodeKindCount <= kInt32Size * kBitsPerByte);

inline constexpr CodeKindFlag CodeKindToCodeKindFlag(CodeKind kind) {
#define V(name) kind == CodeKind::name ? CodeKindFlag::name:
  return CODE_KIND_LIST(V) CodeKindFlag::INTERPRETED_FUNCTION;
#undef V
}

// CodeKinds represents a set of CodeKind.
using CodeKinds = base::Flags<CodeKindFlag>;
DEFINE_OPERATORS_FOR_FLAGS(CodeKinds)

static constexpr CodeKinds kJSFunctionCodeKindsMask{
    CodeKindFlag::INTERPRETED_FUNCTION | CodeKindFlag::TURBOFAN |
    CodeKindFlag::NATIVE_CONTEXT_INDEPENDENT | CodeKindFlag::TURBOPROP};
static constexpr CodeKinds kOptimizedJSFunctionCodeKindsMask{
    CodeKindFlag::TURBOFAN | CodeKindFlag::NATIVE_CONTEXT_INDEPENDENT |
    CodeKindFlag::TURBOPROP};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_CODE_KIND_H_
