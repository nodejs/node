// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/code-factory.h"

#include "src/builtins/builtins-descriptors.h"
#include "src/ic/ic.h"
#include "src/init/bootstrapper.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

// static
Handle<Code> CodeFactory::RuntimeCEntry(Isolate* isolate, int result_size) {
  return CodeFactory::CEntry(isolate, result_size);
}

// static
Handle<Code> CodeFactory::CEntry(Isolate* isolate, int result_size,
                                 ArgvMode argv_mode, bool builtin_exit_frame) {
  // Aliases for readability below.
  const int rs = result_size;
  const ArgvMode am = argv_mode;
  const bool be = builtin_exit_frame;

  if (rs == 1 && am == ArgvMode::kStack && !be) {
    return BUILTIN_CODE(isolate, CEntry_Return1_ArgvOnStack_NoBuiltinExit);
  } else if (rs == 1 && am == ArgvMode::kStack && be) {
    return BUILTIN_CODE(isolate, CEntry_Return1_ArgvOnStack_BuiltinExit);
  } else if (rs == 1 && am == ArgvMode::kRegister && !be) {
    return BUILTIN_CODE(isolate, CEntry_Return1_ArgvInRegister_NoBuiltinExit);
  } else if (rs == 2 && am == ArgvMode::kStack && !be) {
    return BUILTIN_CODE(isolate, CEntry_Return2_ArgvOnStack_NoBuiltinExit);
  } else if (rs == 2 && am == ArgvMode::kStack && be) {
    return BUILTIN_CODE(isolate, CEntry_Return2_ArgvOnStack_BuiltinExit);
  } else if (rs == 2 && am == ArgvMode::kRegister && !be) {
    return BUILTIN_CODE(isolate, CEntry_Return2_ArgvInRegister_NoBuiltinExit);
  }

  UNREACHABLE();
}

// static
Callable CodeFactory::ApiGetter(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kCallApiGetter);
}

// static
Callable CodeFactory::CallApiCallback(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kCallApiCallback);
}

// static
Callable CodeFactory::LoadGlobalIC(Isolate* isolate, TypeofMode typeof_mode) {
  return typeof_mode == TypeofMode::kNotInside
             ? Builtins::CallableFor(isolate, Builtin::kLoadGlobalICTrampoline)
             : Builtins::CallableFor(
                   isolate, Builtin::kLoadGlobalICInsideTypeofTrampoline);
}

// static
Callable CodeFactory::LoadGlobalICInOptimizedCode(Isolate* isolate,
                                                  TypeofMode typeof_mode) {
  return typeof_mode == TypeofMode::kNotInside
             ? Builtins::CallableFor(isolate, Builtin::kLoadGlobalIC)
             : Builtins::CallableFor(isolate,
                                     Builtin::kLoadGlobalICInsideTypeof);
}

Callable CodeFactory::DefineNamedOwnIC(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kDefineNamedOwnICTrampoline);
}

Callable CodeFactory::DefineNamedOwnICInOptimizedCode(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kDefineNamedOwnIC);
}

// static
Callable CodeFactory::NonPrimitiveToPrimitive(Isolate* isolate,
                                              ToPrimitiveHint hint) {
  return Callable(isolate->builtins()->NonPrimitiveToPrimitive(hint),
                  TypeConversionDescriptor{});
}

// static
Callable CodeFactory::OrdinaryToPrimitive(Isolate* isolate,
                                          OrdinaryToPrimitiveHint hint) {
  return Callable(isolate->builtins()->OrdinaryToPrimitive(hint),
                  TypeConversionDescriptor{});
}

// static
Callable CodeFactory::StringAdd(Isolate* isolate, StringAddFlags flags) {
  switch (flags) {
    case STRING_ADD_CHECK_NONE:
      return Builtins::CallableFor(isolate, Builtin::kStringAdd_CheckNone);
    case STRING_ADD_CONVERT_LEFT:
      return Builtins::CallableFor(isolate, Builtin::kStringAddConvertLeft);
    case STRING_ADD_CONVERT_RIGHT:
      return Builtins::CallableFor(isolate, Builtin::kStringAddConvertRight);
  }
  UNREACHABLE();
}

// static
Callable CodeFactory::ResumeGenerator(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kResumeGeneratorTrampoline);
}

// static
Callable CodeFactory::FastNewFunctionContext(Isolate* isolate,
                                             ScopeType scope_type) {
  switch (scope_type) {
    case ScopeType::EVAL_SCOPE:
      return Builtins::CallableFor(isolate,
                                   Builtin::kFastNewFunctionContextEval);
    case ScopeType::FUNCTION_SCOPE:
      return Builtins::CallableFor(isolate,
                                   Builtin::kFastNewFunctionContextFunction);
    default:
      UNREACHABLE();
  }
}

// static
Callable CodeFactory::Call(Isolate* isolate, ConvertReceiverMode mode) {
  return Callable(isolate->builtins()->Call(mode), CallTrampolineDescriptor{});
}

// static
Callable CodeFactory::Call_WithFeedback(Isolate* isolate,
                                        ConvertReceiverMode mode) {
  switch (mode) {
    case ConvertReceiverMode::kNullOrUndefined:
      return Builtins::CallableFor(
          isolate, Builtin::kCall_ReceiverIsNullOrUndefined_WithFeedback);
    case ConvertReceiverMode::kNotNullOrUndefined:
      return Builtins::CallableFor(
          isolate, Builtin::kCall_ReceiverIsNotNullOrUndefined_WithFeedback);
    case ConvertReceiverMode::kAny:
      return Builtins::CallableFor(isolate,
                                   Builtin::kCall_ReceiverIsAny_WithFeedback);
  }
  UNREACHABLE();
}

// static
Callable CodeFactory::CallWithArrayLike(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kCallWithArrayLike);
}

// static
Callable CodeFactory::CallWithSpread(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kCallWithSpread);
}

// static
Callable CodeFactory::CallFunction(Isolate* isolate, ConvertReceiverMode mode) {
  return Callable(isolate->builtins()->CallFunction(mode),
                  CallTrampolineDescriptor{});
}

// static
Callable CodeFactory::CallVarargs(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kCallVarargs);
}

// static
Callable CodeFactory::CallForwardVarargs(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kCallForwardVarargs);
}

// static
Callable CodeFactory::CallFunctionForwardVarargs(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kCallFunctionForwardVarargs);
}

// static
Callable CodeFactory::Construct(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kConstruct);
}

// static
Callable CodeFactory::ConstructWithSpread(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kConstructWithSpread);
}

// static
Callable CodeFactory::ConstructFunction(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kConstructFunction);
}

// static
Callable CodeFactory::ConstructVarargs(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kConstructVarargs);
}

// static
Callable CodeFactory::ConstructForwardVarargs(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtin::kConstructForwardVarargs);
}

// static
Callable CodeFactory::ConstructFunctionForwardVarargs(Isolate* isolate) {
  return Builtins::CallableFor(isolate,
                               Builtin::kConstructFunctionForwardVarargs);
}

// static
Callable CodeFactory::InterpreterPushArgsThenCall(
    Isolate* isolate, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  switch (mode) {
    case InterpreterPushArgsMode::kArrayFunction:
      // There is no special-case handling of calls to Array. They will all go
      // through the kOther case below.
      UNREACHABLE();
    case InterpreterPushArgsMode::kWithFinalSpread:
      return Builtins::CallableFor(
          isolate, Builtin::kInterpreterPushArgsThenCallWithFinalSpread);
    case InterpreterPushArgsMode::kOther:
      switch (receiver_mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return Builtins::CallableFor(
              isolate, Builtin::kInterpreterPushUndefinedAndArgsThenCall);
        case ConvertReceiverMode::kNotNullOrUndefined:
        case ConvertReceiverMode::kAny:
          return Builtins::CallableFor(isolate,
                                       Builtin::kInterpreterPushArgsThenCall);
      }
  }
  UNREACHABLE();
}

// static
Callable CodeFactory::InterpreterPushArgsThenConstruct(
    Isolate* isolate, InterpreterPushArgsMode mode) {
  switch (mode) {
    case InterpreterPushArgsMode::kArrayFunction:
      return Builtins::CallableFor(
          isolate, Builtin::kInterpreterPushArgsThenConstructArrayFunction);
    case InterpreterPushArgsMode::kWithFinalSpread:
      return Builtins::CallableFor(
          isolate, Builtin::kInterpreterPushArgsThenConstructWithFinalSpread);
    case InterpreterPushArgsMode::kOther:
      return Builtins::CallableFor(isolate,
                                   Builtin::kInterpreterPushArgsThenConstruct);
  }
  UNREACHABLE();
}

// static
Callable CodeFactory::InterpreterCEntry(Isolate* isolate, int result_size) {
  Handle<Code> code =
      CodeFactory::CEntry(isolate, result_size, ArgvMode::kRegister);
  if (result_size == 1) {
    return Callable(code, InterpreterCEntry1Descriptor{});
  } else {
    DCHECK_EQ(result_size, 2);
    return Callable(code, InterpreterCEntry2Descriptor{});
  }
}

// static
Callable CodeFactory::InterpreterOnStackReplacement(Isolate* isolate) {
  return Builtins::CallableFor(isolate,
                               Builtin::kInterpreterOnStackReplacement);
}

// static
Callable CodeFactory::InterpreterOnStackReplacement_ToBaseline(
    Isolate* isolate) {
  return Builtins::CallableFor(
      isolate, Builtin::kInterpreterOnStackReplacement_ToBaseline);
}

// static
Callable CodeFactory::ArrayNoArgumentConstructor(
    Isolate* isolate, ElementsKind kind,
    AllocationSiteOverrideMode override_mode) {
#define CASE(kind_caps, kind_camel, mode_camel) \
  case kind_caps:                               \
    return Builtins::CallableFor(               \
        isolate,                                \
        Builtin::kArrayNoArgumentConstructor_##kind_camel##_##mode_camel);
  if (override_mode == DONT_OVERRIDE && AllocationSite::ShouldTrack(kind)) {
    DCHECK(IsSmiElementsKind(kind));
    switch (kind) {
      CASE(PACKED_SMI_ELEMENTS, PackedSmi, DontOverride);
      CASE(HOLEY_SMI_ELEMENTS, HoleySmi, DontOverride);
      default:
        UNREACHABLE();
    }
  } else {
    DCHECK(override_mode == DISABLE_ALLOCATION_SITES ||
           !AllocationSite::ShouldTrack(kind));
    switch (kind) {
      CASE(PACKED_SMI_ELEMENTS, PackedSmi, DisableAllocationSites);
      CASE(HOLEY_SMI_ELEMENTS, HoleySmi, DisableAllocationSites);
      CASE(PACKED_ELEMENTS, Packed, DisableAllocationSites);
      CASE(HOLEY_ELEMENTS, Holey, DisableAllocationSites);
      CASE(PACKED_DOUBLE_ELEMENTS, PackedDouble, DisableAllocationSites);
      CASE(HOLEY_DOUBLE_ELEMENTS, HoleyDouble, DisableAllocationSites);
      default:
        UNREACHABLE();
    }
  }
#undef CASE
}

// static
Callable CodeFactory::ArraySingleArgumentConstructor(
    Isolate* isolate, ElementsKind kind,
    AllocationSiteOverrideMode override_mode) {
#define CASE(kind_caps, kind_camel, mode_camel) \
  case kind_caps:                               \
    return Builtins::CallableFor(               \
        isolate,                                \
        Builtin::kArraySingleArgumentConstructor_##kind_camel##_##mode_camel)
  if (override_mode == DONT_OVERRIDE && AllocationSite::ShouldTrack(kind)) {
    DCHECK(IsSmiElementsKind(kind));
    switch (kind) {
      CASE(PACKED_SMI_ELEMENTS, PackedSmi, DontOverride);
      CASE(HOLEY_SMI_ELEMENTS, HoleySmi, DontOverride);
      default:
        UNREACHABLE();
    }
  } else {
    DCHECK(override_mode == DISABLE_ALLOCATION_SITES ||
           !AllocationSite::ShouldTrack(kind));
    switch (kind) {
      CASE(PACKED_SMI_ELEMENTS, PackedSmi, DisableAllocationSites);
      CASE(HOLEY_SMI_ELEMENTS, HoleySmi, DisableAllocationSites);
      CASE(PACKED_ELEMENTS, Packed, DisableAllocationSites);
      CASE(HOLEY_ELEMENTS, Holey, DisableAllocationSites);
      CASE(PACKED_DOUBLE_ELEMENTS, PackedDouble, DisableAllocationSites);
      CASE(HOLEY_DOUBLE_ELEMENTS, HoleyDouble, DisableAllocationSites);
      default:
        UNREACHABLE();
    }
  }
#undef CASE
}

#ifdef V8_IS_TSAN
// static
Builtin CodeFactory::GetTSANStoreStub(SaveFPRegsMode fp_mode, int size,
                                      std::memory_order order) {
  if (order == std::memory_order_relaxed) {
    if (size == kInt8Size) {
      return fp_mode == SaveFPRegsMode::kIgnore
                 ? Builtin::kTSANRelaxedStore8IgnoreFP
                 : Builtin::kTSANRelaxedStore8SaveFP;
    } else if (size == kInt16Size) {
      return fp_mode == SaveFPRegsMode::kIgnore
                 ? Builtin::kTSANRelaxedStore16IgnoreFP
                 : Builtin::kTSANRelaxedStore16SaveFP;
    } else if (size == kInt32Size) {
      return fp_mode == SaveFPRegsMode::kIgnore
                 ? Builtin::kTSANRelaxedStore32IgnoreFP
                 : Builtin::kTSANRelaxedStore32SaveFP;
    } else {
      CHECK_EQ(size, kInt64Size);
      return fp_mode == SaveFPRegsMode::kIgnore
                 ? Builtin::kTSANRelaxedStore64IgnoreFP
                 : Builtin::kTSANRelaxedStore64SaveFP;
    }
  } else {
    DCHECK_EQ(order, std::memory_order_seq_cst);
    if (size == kInt8Size) {
      return fp_mode == SaveFPRegsMode::kIgnore
                 ? Builtin::kTSANSeqCstStore8IgnoreFP
                 : Builtin::kTSANSeqCstStore8SaveFP;
    } else if (size == kInt16Size) {
      return fp_mode == SaveFPRegsMode::kIgnore
                 ? Builtin::kTSANSeqCstStore16IgnoreFP
                 : Builtin::kTSANSeqCstStore16SaveFP;
    } else if (size == kInt32Size) {
      return fp_mode == SaveFPRegsMode::kIgnore
                 ? Builtin::kTSANSeqCstStore32IgnoreFP
                 : Builtin::kTSANSeqCstStore32SaveFP;
    } else {
      CHECK_EQ(size, kInt64Size);
      return fp_mode == SaveFPRegsMode::kIgnore
                 ? Builtin::kTSANSeqCstStore64IgnoreFP
                 : Builtin::kTSANSeqCstStore64SaveFP;
    }
  }
}

// static
Builtin CodeFactory::GetTSANRelaxedLoadStub(SaveFPRegsMode fp_mode, int size) {
  if (size == kInt32Size) {
    return fp_mode == SaveFPRegsMode::kIgnore
               ? Builtin::kTSANRelaxedLoad32IgnoreFP
               : Builtin::kTSANRelaxedLoad32SaveFP;
  } else {
    CHECK_EQ(size, kInt64Size);
    return fp_mode == SaveFPRegsMode::kIgnore
               ? Builtin::kTSANRelaxedLoad64IgnoreFP
               : Builtin::kTSANRelaxedLoad64SaveFP;
  }
}
#endif  // V8_IS_TSAN

}  // namespace internal
}  // namespace v8
