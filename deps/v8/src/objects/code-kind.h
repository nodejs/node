// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CODE_KIND_H_
#define V8_OBJECTS_CODE_KIND_H_

#include "src/base/flags.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {

// TODO(jgruber,rmcilroy): Rename OPTIMIZED_FUNCTION once we've fully
// disambiguated Turboprop, Turbofan, and NCI code kinds.
// TODO(jgruber): Rename STUB to DEOPT_ENTRIES_OR_FOR_TESTING, or split it into
// DEOPT_ENTRIES and FOR_TESTING, or convert DEOPT_ENTRIES into a builtin.
#define CODE_KIND_LIST(V)  \
  V(OPTIMIZED_FUNCTION)    \
  V(BYTECODE_HANDLER)      \
  V(STUB)                  \
  V(BUILTIN)               \
  V(REGEXP)                \
  V(WASM_FUNCTION)         \
  V(WASM_TO_CAPI_FUNCTION) \
  V(WASM_TO_JS_FUNCTION)   \
  V(JS_TO_WASM_FUNCTION)   \
  V(JS_TO_JS_FUNCTION)     \
  V(C_WASM_ENTRY)          \
  V(INTERPRETED_FUNCTION)  \
  V(NATIVE_CONTEXT_INDEPENDENT)

enum class CodeKind {
#define DEFINE_CODE_KIND_ENUM(name) name,
  CODE_KIND_LIST(DEFINE_CODE_KIND_ENUM)
#undef DEFINE_CODE_KIND_ENUM
};

#define V(...) +1
static constexpr int kCodeKindCount = CODE_KIND_LIST(V);
#undef V

const char* CodeKindToString(CodeKind kind);

inline constexpr bool CodeKindIsInterpretedJSFunction(CodeKind kind) {
  return kind == CodeKind::INTERPRETED_FUNCTION;
}

inline constexpr bool CodeKindIsNativeContextIndependentJSFunction(
    CodeKind kind) {
  return kind == CodeKind::NATIVE_CONTEXT_INDEPENDENT;
}

inline constexpr bool CodeKindIsOptimizedJSFunction(CodeKind kind) {
  return kind == CodeKind::OPTIMIZED_FUNCTION ||
         kind == CodeKind::NATIVE_CONTEXT_INDEPENDENT;
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

inline constexpr bool CodeKindChecksOptimizationMarker(CodeKind kind) {
  return kind == CodeKind::INTERPRETED_FUNCTION ||
         kind == CodeKind::NATIVE_CONTEXT_INDEPENDENT;
}

// The optimization marker field on the feedback vector has a dual purpose of
// controlling the tier-up workflow, and caching the produced code object for
// access from multiple closures. The marker is not used for all code kinds
// though, in particular it is not used when generating NCI code.
inline constexpr bool CodeKindIsStoredInOptimizedCodeCache(CodeKind kind) {
  return kind == CodeKind::OPTIMIZED_FUNCTION;
}

inline CodeKind CodeKindForTopTier() { return CodeKind::OPTIMIZED_FUNCTION; }

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
    CodeKindFlag::INTERPRETED_FUNCTION | CodeKindFlag::OPTIMIZED_FUNCTION |
    CodeKindFlag::NATIVE_CONTEXT_INDEPENDENT};
static constexpr CodeKinds kOptimizedJSFunctionCodeKindsMask{
    CodeKindFlag::OPTIMIZED_FUNCTION |
    CodeKindFlag::NATIVE_CONTEXT_INDEPENDENT};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_CODE_KIND_H_
