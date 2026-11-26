// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/protectors.h"

#include "src/execution/isolate-inl.h"
#include "src/execution/protectors-inl.h"
#include "src/handles/handles-inl.h"
#include "src/objects/contexts.h"
#include "src/objects/property-cell.h"
#include "src/objects/smi.h"
#include "src/tracing/trace-event.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

namespace {

void TraceProtectorInvalidation(const char* protector_name) {
  DCHECK(v8_flags.trace_protector_invalidation);
  static constexpr char kInvalidateProtectorTracingCategory[] =
      "V8.InvalidateProtector";
  [[maybe_unused]] static constexpr char kInvalidateProtectorTracingArg[] =
      "protector-name";

  DCHECK(v8_flags.trace_protector_invalidation);

  // TODO(jgruber): Remove the PrintF once tracing can output to stdout.
  i::PrintF("Invalidating protector cell %s\n", protector_name);
  TRACE_EVENT_INSTANT("v8", kInvalidateProtectorTracingCategory,
                      kInvalidateProtectorTracingArg, protector_name);
}

// Static asserts to ensure we have a use counter for every protector. If this
// fails, add the use counter in V8 and chromium. Note: IsDefined is not
// strictly needed but clarifies the intent of the static assert.
constexpr bool IsDefined(v8::Isolate::UseCounterFeature) { return true; }
#define V(Name, ...) \
  static_assert(IsDefined(v8::Isolate::kInvalidated##Name##Protector));

DECLARED_PROTECTORS_ON_ISOLATE(V)
#undef V

}  // namespace

#define INVALIDATE_PROTECTOR_ON_ISOLATE_DEFINITION(name, unused_index, cell) \
  void Protectors::Invalidate##name(Isolate* isolate) {                      \
    DCHECK(IsSmi(isolate->factory()->cell()->value()));                      \
    DCHECK(Is##name##Intact(isolate));                                       \
    if (v8_flags.trace_protector_invalidation) {                             \
      TraceProtectorInvalidation(#name);                                     \
    }                                                                        \
    isolate->CountUsage(v8::Isolate::kInvalidated##name##Protector);         \
    isolate->factory()->cell()->InvalidateProtector(isolate);                \
    DCHECK(!Is##name##Intact(isolate));                                      \
  }
DECLARED_PROTECTORS_ON_ISOLATE(INVALIDATE_PROTECTOR_ON_ISOLATE_DEFINITION)
#undef INVALIDATE_PROTECTOR_ON_ISOLATE_DEFINITION

namespace {

enum class ProtectorId {
#define DEFINE_PROTECTOR_ID(name, ...) k##name,
  DECLARED_PROTECTORS_ON_ISOLATE(DEFINE_PROTECTOR_ID)
#undef DEFINE_PROTECTOR_ID
};

constexpr bool ShouldResetProtectorForReprl(ProtectorId id) {
  switch (id) {
    // Protectors for JS object/prototype state: each REPRL run gets a fresh
    // NativeContext, and lingering callbacks remain memory-safe after reset.
    case ProtectorId::kArrayIteratorLookupChain:
    case ProtectorId::kArraySpeciesLookupChain:
    case ProtectorId::kIsConcatSpreadableLookupChain:
    case ProtectorId::kMapIteratorLookupChain:
    case ProtectorId::kNoElements:
    case ProtectorId::kNumberStringNotRegexpLike:
    case ProtectorId::kPromiseResolveLookupChain:
    case ProtectorId::kPromiseSpeciesLookupChain:
    case ProtectorId::kPromiseThenLookupChain:
    case ProtectorId::kRegExpSpeciesLookupChain:
    case ProtectorId::kSetIteratorLookupChain:
    case ProtectorId::kStringIteratorLookupChain:
    case ProtectorId::kStringWrapperToPrimitive:
    case ProtectorId::kTypedArraySpeciesLookupChain:
      return true;

    // Deopt-loop guard; not tied to any surviving object or state.
    case ProtectorId::kStringLengthOverflowLookupChain:
      return true;

    // Undetectable maps are created per NativeContext; readers only affect
    // boolean/typeof fast paths (no type narrowing or heap invariants).
    case ProtectorId::kNoUndetectableObjects:
      return true;

    // Guards only freshness of cached JSDate fields (always Smi-in-range or
    // NaN); lingering callbacks can at worst read stale values.
    case ProtectorId::kNoDateTimeConfigurationChange:
      return true;

    // Isolate-level state that can remain active across iterations.
    case ProtectorId::kNoProfiling:
    case ProtectorId::kPromiseHook:
      return false;

    // Embedder DOM IC protector, conservatively kept unreset.
    case ProtectorId::kMegaDOM:
      return false;

    // Once invalidated, detaching stops updating views in place; resetting
    // causes UAF in lingering callbacks and --verify-heap crashes before GC.
    case ProtectorId::kArrayBufferDetaching:
      return false;

    // Once invalidated, JIT code checks array buffer mutability before
    // writing; resetting would allow JIT code to write to lingering immutable
    // array buffers, causing data races if shared with Workers.
    case ProtectorId::kArrayBufferMutable:
      return false;

      // Default new protectors to false unless audited for REPRL reset safety.
  }
  UNREACHABLE();
}

}  // namespace

void Protectors::ResetForReprl(Isolate* isolate) {
#define MAYBE_RESET_PROTECTOR(name, unused_index, cell)               \
  if constexpr (ShouldResetProtectorForReprl(ProtectorId::k##name)) { \
    isolate->factory()->cell()->ResetProtectorForReprl();             \
  }
  DECLARED_PROTECTORS_ON_ISOLATE(MAYBE_RESET_PROTECTOR)
#undef MAYBE_RESET_PROTECTOR
}

}  // namespace internal
}  // namespace v8
