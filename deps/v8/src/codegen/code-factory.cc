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

#define CENTRY_CODE(RS, SD, AM, BE) \
  BUILTIN_CODE(isolate, CEntry_##RS##_##SD##_##AM##_##BE)

// static
Handle<Code> CodeFactory::CEntry(Isolate* isolate, int result_size,
                                 SaveFPRegsMode save_doubles,
                                 ArgvMode argv_mode, bool builtin_exit_frame) {
  // Aliases for readability below.
  const int rs = result_size;
  const SaveFPRegsMode sd = save_doubles;
  const ArgvMode am = argv_mode;
  const bool be = builtin_exit_frame;

  if (rs == 1 && sd == kDontSaveFPRegs && am == kArgvOnStack && !be) {
    return CENTRY_CODE(Return1, DontSaveFPRegs, ArgvOnStack, NoBuiltinExit);
  } else if (rs == 1 && sd == kDontSaveFPRegs && am == kArgvOnStack && be) {
    return CENTRY_CODE(Return1, DontSaveFPRegs, ArgvOnStack, BuiltinExit);
  } else if (rs == 1 && sd == kDontSaveFPRegs && am == kArgvInRegister && !be) {
    return CENTRY_CODE(Return1, DontSaveFPRegs, ArgvInRegister, NoBuiltinExit);
  } else if (rs == 1 && sd == kSaveFPRegs && am == kArgvOnStack && !be) {
    return CENTRY_CODE(Return1, SaveFPRegs, ArgvOnStack, NoBuiltinExit);
  } else if (rs == 1 && sd == kSaveFPRegs && am == kArgvOnStack && be) {
    return CENTRY_CODE(Return1, SaveFPRegs, ArgvOnStack, BuiltinExit);
  } else if (rs == 2 && sd == kDontSaveFPRegs && am == kArgvOnStack && !be) {
    return CENTRY_CODE(Return2, DontSaveFPRegs, ArgvOnStack, NoBuiltinExit);
  } else if (rs == 2 && sd == kDontSaveFPRegs && am == kArgvOnStack && be) {
    return CENTRY_CODE(Return2, DontSaveFPRegs, ArgvOnStack, BuiltinExit);
  } else if (rs == 2 && sd == kDontSaveFPRegs && am == kArgvInRegister && !be) {
    return CENTRY_CODE(Return2, DontSaveFPRegs, ArgvInRegister, NoBuiltinExit);
  } else if (rs == 2 && sd == kSaveFPRegs && am == kArgvOnStack && !be) {
    return CENTRY_CODE(Return2, SaveFPRegs, ArgvOnStack, NoBuiltinExit);
  } else if (rs == 2 && sd == kSaveFPRegs && am == kArgvOnStack && be) {
    return CENTRY_CODE(Return2, SaveFPRegs, ArgvOnStack, BuiltinExit);
  }

  UNREACHABLE();
}

#undef CENTRY_CODE

// static
Callable CodeFactory::ApiGetter(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kCallApiGetter);
}

// static
Callable CodeFactory::CallApiCallback(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kCallApiCallback);
}

// static
Callable CodeFactory::LoadGlobalIC(Isolate* isolate, TypeofMode typeof_mode) {
  return typeof_mode == NOT_INSIDE_TYPEOF
             ? Builtins::CallableFor(isolate, Builtins::kLoadGlobalICTrampoline)
             : Builtins::CallableFor(
                   isolate, Builtins::kLoadGlobalICInsideTypeofTrampoline);
}

// static
Callable CodeFactory::LoadGlobalICInOptimizedCode(Isolate* isolate,
                                                  TypeofMode typeof_mode) {
  return typeof_mode == NOT_INSIDE_TYPEOF
             ? Builtins::CallableFor(isolate, Builtins::kLoadGlobalIC)
             : Builtins::CallableFor(isolate,
                                     Builtins::kLoadGlobalICInsideTypeof);
}

Callable CodeFactory::StoreOwnIC(Isolate* isolate) {
  // TODO(ishell): Currently we use StoreOwnIC only for storing properties that
  // already exist in the boilerplate therefore we can use StoreIC.
  return Builtins::CallableFor(isolate, Builtins::kStoreICTrampoline);
}

Callable CodeFactory::StoreOwnICInOptimizedCode(Isolate* isolate) {
  // TODO(ishell): Currently we use StoreOwnIC only for storing properties that
  // already exist in the boilerplate therefore we can use StoreIC.
  return Builtins::CallableFor(isolate, Builtins::kStoreIC);
}

Callable CodeFactory::KeyedStoreIC_SloppyArguments(Isolate* isolate,
                                                   KeyedAccessStoreMode mode) {
  Builtins::Name builtin_index;
  switch (mode) {
    case STANDARD_STORE:
      builtin_index = Builtins::kKeyedStoreIC_SloppyArguments_Standard;
      break;
    case STORE_AND_GROW_HANDLE_COW:
      builtin_index =
          Builtins::kKeyedStoreIC_SloppyArguments_GrowNoTransitionHandleCOW;
      break;
    case STORE_IGNORE_OUT_OF_BOUNDS:
      builtin_index =
          Builtins::kKeyedStoreIC_SloppyArguments_NoTransitionIgnoreOOB;
      break;
    case STORE_HANDLE_COW:
      builtin_index =
          Builtins::kKeyedStoreIC_SloppyArguments_NoTransitionHandleCOW;
      break;
    default:
      UNREACHABLE();
  }
  return isolate->builtins()->CallableFor(isolate, builtin_index);
}

Callable CodeFactory::ElementsTransitionAndStore(Isolate* isolate,
                                                 KeyedAccessStoreMode mode) {
  Builtins::Name builtin_index;
  switch (mode) {
    case STANDARD_STORE:
      builtin_index = Builtins::kElementsTransitionAndStore_Standard;
      break;
    case STORE_AND_GROW_HANDLE_COW:
      builtin_index =
          Builtins::kElementsTransitionAndStore_GrowNoTransitionHandleCOW;
      break;
    case STORE_IGNORE_OUT_OF_BOUNDS:
      builtin_index =
          Builtins::kElementsTransitionAndStore_NoTransitionIgnoreOOB;
      break;
    case STORE_HANDLE_COW:
      builtin_index =
          Builtins::kElementsTransitionAndStore_NoTransitionHandleCOW;
      break;
    default:
      UNREACHABLE();
  }
  return isolate->builtins()->CallableFor(isolate, builtin_index);
}

Callable CodeFactory::StoreFastElementIC(Isolate* isolate,
                                         KeyedAccessStoreMode mode) {
  Builtins::Name builtin_index;
  switch (mode) {
    case STANDARD_STORE:
      builtin_index = Builtins::kStoreFastElementIC_Standard;
      break;
    case STORE_AND_GROW_HANDLE_COW:
      builtin_index = Builtins::kStoreFastElementIC_GrowNoTransitionHandleCOW;
      break;
    case STORE_IGNORE_OUT_OF_BOUNDS:
      builtin_index = Builtins::kStoreFastElementIC_NoTransitionIgnoreOOB;
      break;
    case STORE_HANDLE_COW:
      builtin_index = Builtins::kStoreFastElementIC_NoTransitionHandleCOW;
      break;
    default:
      UNREACHABLE();
  }
  return isolate->builtins()->CallableFor(isolate, builtin_index);
}

// static
Callable CodeFactory::BinaryOperation(Isolate* isolate, Operation op) {
  switch (op) {
    case Operation::kShiftRight:
      return Builtins::CallableFor(isolate, Builtins::kShiftRight);
    case Operation::kShiftLeft:
      return Builtins::CallableFor(isolate, Builtins::kShiftLeft);
    case Operation::kShiftRightLogical:
      return Builtins::CallableFor(isolate, Builtins::kShiftRightLogical);
    case Operation::kAdd:
      return Builtins::CallableFor(isolate, Builtins::kAdd);
    case Operation::kSubtract:
      return Builtins::CallableFor(isolate, Builtins::kSubtract);
    case Operation::kMultiply:
      return Builtins::CallableFor(isolate, Builtins::kMultiply);
    case Operation::kDivide:
      return Builtins::CallableFor(isolate, Builtins::kDivide);
    case Operation::kModulus:
      return Builtins::CallableFor(isolate, Builtins::kModulus);
    case Operation::kBitwiseOr:
      return Builtins::CallableFor(isolate, Builtins::kBitwiseOr);
    case Operation::kBitwiseAnd:
      return Builtins::CallableFor(isolate, Builtins::kBitwiseAnd);
    case Operation::kBitwiseXor:
      return Builtins::CallableFor(isolate, Builtins::kBitwiseXor);
    default:
      break;
  }
  UNREACHABLE();
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
      return Builtins::CallableFor(isolate, Builtins::kStringAdd_CheckNone);
    case STRING_ADD_CONVERT_LEFT:
      return Builtins::CallableFor(isolate, Builtins::kStringAddConvertLeft);
    case STRING_ADD_CONVERT_RIGHT:
      return Builtins::CallableFor(isolate, Builtins::kStringAddConvertRight);
  }
  UNREACHABLE();
}

// static
Callable CodeFactory::ResumeGenerator(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kResumeGeneratorTrampoline);
}

// static
Callable CodeFactory::FrameDropperTrampoline(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kFrameDropperTrampoline);
}

// static
Callable CodeFactory::HandleDebuggerStatement(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kHandleDebuggerStatement);
}

// static
Callable CodeFactory::FastNewFunctionContext(Isolate* isolate,
                                             ScopeType scope_type) {
  switch (scope_type) {
    case ScopeType::EVAL_SCOPE:
      return Builtins::CallableFor(isolate,
                                   Builtins::kFastNewFunctionContextEval);
    case ScopeType::FUNCTION_SCOPE:
      return Builtins::CallableFor(isolate,
                                   Builtins::kFastNewFunctionContextFunction);
    default:
      UNREACHABLE();
  }
}

// static
Callable CodeFactory::ArgumentAdaptor(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kArgumentsAdaptorTrampoline);
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
          isolate, Builtins::kCall_ReceiverIsNullOrUndefined_WithFeedback);
    case ConvertReceiverMode::kNotNullOrUndefined:
      return Builtins::CallableFor(
          isolate, Builtins::kCall_ReceiverIsNotNullOrUndefined_WithFeedback);
    case ConvertReceiverMode::kAny:
      return Builtins::CallableFor(isolate,
                                   Builtins::kCall_ReceiverIsAny_WithFeedback);
  }
  UNREACHABLE();
}

// static
Callable CodeFactory::CallWithArrayLike(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kCallWithArrayLike);
}

// static
Callable CodeFactory::CallWithSpread(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kCallWithSpread);
}

// static
Callable CodeFactory::CallFunction(Isolate* isolate, ConvertReceiverMode mode) {
  return Callable(isolate->builtins()->CallFunction(mode),
                  CallTrampolineDescriptor{});
}

// static
Callable CodeFactory::CallVarargs(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kCallVarargs);
}

// static
Callable CodeFactory::CallForwardVarargs(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kCallForwardVarargs);
}

// static
Callable CodeFactory::CallFunctionForwardVarargs(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kCallFunctionForwardVarargs);
}

// static
Callable CodeFactory::Construct(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kConstruct);
}

// static
Callable CodeFactory::ConstructWithSpread(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kConstructWithSpread);
}

// static
Callable CodeFactory::ConstructFunction(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kConstructFunction);
}

// static
Callable CodeFactory::ConstructVarargs(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kConstructVarargs);
}

// static
Callable CodeFactory::ConstructForwardVarargs(Isolate* isolate) {
  return Builtins::CallableFor(isolate, Builtins::kConstructForwardVarargs);
}

// static
Callable CodeFactory::ConstructFunctionForwardVarargs(Isolate* isolate) {
  return Builtins::CallableFor(isolate,
                               Builtins::kConstructFunctionForwardVarargs);
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
          isolate, Builtins::kInterpreterPushArgsThenCallWithFinalSpread);
    case InterpreterPushArgsMode::kOther:
      switch (receiver_mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return Builtins::CallableFor(
              isolate, Builtins::kInterpreterPushUndefinedAndArgsThenCall);
        case ConvertReceiverMode::kNotNullOrUndefined:
        case ConvertReceiverMode::kAny:
          return Builtins::CallableFor(isolate,
                                       Builtins::kInterpreterPushArgsThenCall);
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
          isolate, Builtins::kInterpreterPushArgsThenConstructArrayFunction);
    case InterpreterPushArgsMode::kWithFinalSpread:
      return Builtins::CallableFor(
          isolate, Builtins::kInterpreterPushArgsThenConstructWithFinalSpread);
    case InterpreterPushArgsMode::kOther:
      return Builtins::CallableFor(isolate,
                                   Builtins::kInterpreterPushArgsThenConstruct);
  }
  UNREACHABLE();
}

// static
Callable CodeFactory::InterpreterCEntry(Isolate* isolate, int result_size) {
  // Note: If we ever use fpregs in the interpreter then we will need to
  // save fpregs too.
  Handle<Code> code = CodeFactory::CEntry(isolate, result_size, kDontSaveFPRegs,
                                          kArgvInRegister);
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
                               Builtins::kInterpreterOnStackReplacement);
}

// static
Callable CodeFactory::ArrayNoArgumentConstructor(
    Isolate* isolate, ElementsKind kind,
    AllocationSiteOverrideMode override_mode) {
#define CASE(kind_caps, kind_camel, mode_camel) \
  case kind_caps:                               \
    return Builtins::CallableFor(               \
        isolate,                                \
        Builtins::kArrayNoArgumentConstructor_##kind_camel##_##mode_camel);
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
        Builtins::kArraySingleArgumentConstructor_##kind_camel##_##mode_camel)
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

}  // namespace internal
}  // namespace v8
