// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-factory.h"

#include "src/bootstrapper.h"
#include "src/builtins/builtins-descriptors.h"
#include "src/ic/ic.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

namespace {

// TODO(ishell): make it (const Stub& stub) once CodeStub::GetCode() is const.
template <typename Stub>
Callable make_callable(Stub& stub) {
  typedef typename Stub::Descriptor Descriptor;
  return Callable(stub.GetCode(), Descriptor{});
}

}  // namespace

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
  return Callable(BUILTIN_CODE(isolate, CallApiGetter), ApiGetterDescriptor{});
}

// static
Callable CodeFactory::CallApiCallback(Isolate* isolate, int argc) {
  switch (argc) {
    case 0:
      return Callable(BUILTIN_CODE(isolate, CallApiCallback_Argc0),
                      ApiCallbackDescriptor{});
    case 1:
      return Callable(BUILTIN_CODE(isolate, CallApiCallback_Argc1),
                      ApiCallbackDescriptor{});
    default: {
      CallApiCallbackStub stub(isolate, argc);
      return make_callable(stub);
    }
  }
  UNREACHABLE();
}

// static
Callable CodeFactory::LoadGlobalIC(Isolate* isolate, TypeofMode typeof_mode) {
  return Callable(
      typeof_mode == NOT_INSIDE_TYPEOF
          ? BUILTIN_CODE(isolate, LoadGlobalICTrampoline)
          : BUILTIN_CODE(isolate, LoadGlobalICInsideTypeofTrampoline),
      LoadGlobalDescriptor{});
}

// static
Callable CodeFactory::LoadGlobalICInOptimizedCode(Isolate* isolate,
                                                  TypeofMode typeof_mode) {
  return Callable(typeof_mode == NOT_INSIDE_TYPEOF
                      ? BUILTIN_CODE(isolate, LoadGlobalIC)
                      : BUILTIN_CODE(isolate, LoadGlobalICInsideTypeof),
                  LoadGlobalWithVectorDescriptor{});
}

Callable CodeFactory::StoreOwnIC(Isolate* isolate) {
  // TODO(ishell): Currently we use StoreOwnIC only for storing properties that
  // already exist in the boilerplate therefore we can use StoreIC.
  return Callable(BUILTIN_CODE(isolate, StoreICTrampoline), StoreDescriptor{});
}

Callable CodeFactory::StoreOwnICInOptimizedCode(Isolate* isolate) {
  // TODO(ishell): Currently we use StoreOwnIC only for storing properties that
  // already exist in the boilerplate therefore we can use StoreIC.
  return Callable(BUILTIN_CODE(isolate, StoreIC), StoreWithVectorDescriptor{});
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
      return Builtins::CallableFor(isolate, Builtins::kStringAdd_ConvertLeft);
    case STRING_ADD_CONVERT_RIGHT:
      return Builtins::CallableFor(isolate, Builtins::kStringAdd_ConvertRight);
  }
  UNREACHABLE();
}

// static
Callable CodeFactory::ResumeGenerator(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ResumeGeneratorTrampoline),
                  ResumeGeneratorDescriptor{});
}

// static
Callable CodeFactory::FrameDropperTrampoline(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, FrameDropperTrampoline),
                  FrameDropperTrampolineDescriptor{});
}

// static
Callable CodeFactory::HandleDebuggerStatement(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, HandleDebuggerStatement),
                  ContextOnlyDescriptor{});
}

// static
Callable CodeFactory::FastNewFunctionContext(Isolate* isolate,
                                             ScopeType scope_type) {
  return Callable(isolate->builtins()->NewFunctionContext(scope_type),
                  FastNewFunctionContextDescriptor{});
}

// static
Callable CodeFactory::ArgumentAdaptor(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ArgumentsAdaptorTrampoline),
                  ArgumentsAdaptorDescriptor{});
}

// static
Callable CodeFactory::Call(Isolate* isolate, ConvertReceiverMode mode) {
  return Callable(isolate->builtins()->Call(mode), CallTrampolineDescriptor{});
}

// static
Callable CodeFactory::CallWithArrayLike(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, CallWithArrayLike),
                  CallWithArrayLikeDescriptor{});
}

// static
Callable CodeFactory::CallWithSpread(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, CallWithSpread),
                  CallWithSpreadDescriptor{});
}

// static
Callable CodeFactory::CallFunction(Isolate* isolate, ConvertReceiverMode mode) {
  return Callable(isolate->builtins()->CallFunction(mode),
                  CallTrampolineDescriptor{});
}

// static
Callable CodeFactory::CallVarargs(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, CallVarargs), CallVarargsDescriptor{});
}

// static
Callable CodeFactory::CallForwardVarargs(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, CallForwardVarargs),
                  CallForwardVarargsDescriptor{});
}

// static
Callable CodeFactory::CallFunctionForwardVarargs(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, CallFunctionForwardVarargs),
                  CallForwardVarargsDescriptor{});
}

// static
Callable CodeFactory::Construct(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, Construct), JSTrampolineDescriptor{});
}

// static
Callable CodeFactory::ConstructWithSpread(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ConstructWithSpread),
                  ConstructWithSpreadDescriptor{});
}

// static
Callable CodeFactory::ConstructFunction(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ConstructFunction),
                  JSTrampolineDescriptor{});
}

// static
Callable CodeFactory::ConstructVarargs(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ConstructVarargs),
                  ConstructVarargsDescriptor{});
}

// static
Callable CodeFactory::ConstructForwardVarargs(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ConstructForwardVarargs),
                  ConstructForwardVarargsDescriptor{});
}

// static
Callable CodeFactory::ConstructFunctionForwardVarargs(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ConstructFunctionForwardVarargs),
                  ConstructForwardVarargsDescriptor{});
}

// static
Callable CodeFactory::InterpreterPushArgsThenCall(
    Isolate* isolate, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  return Callable(
      isolate->builtins()->InterpreterPushArgsThenCall(receiver_mode, mode),
      InterpreterPushArgsThenCallDescriptor{});
}

// static
Callable CodeFactory::InterpreterPushArgsThenConstruct(
    Isolate* isolate, InterpreterPushArgsMode mode) {
  return Callable(isolate->builtins()->InterpreterPushArgsThenConstruct(mode),
                  InterpreterPushArgsThenConstructDescriptor{});
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
  return Callable(BUILTIN_CODE(isolate, InterpreterOnStackReplacement),
                  ContextOnlyDescriptor{});
}

// static
Callable CodeFactory::ArrayNoArgumentConstructor(
    Isolate* isolate, ElementsKind kind,
    AllocationSiteOverrideMode override_mode) {
#define CASE(kind_caps, kind_camel, mode_camel)                               \
  case kind_caps:                                                             \
    return Callable(                                                          \
        BUILTIN_CODE(isolate,                                                 \
                     ArrayNoArgumentConstructor_##kind_camel##_##mode_camel), \
        ArrayNoArgumentConstructorDescriptor{})
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
#define CASE(kind_caps, kind_camel, mode_camel)                          \
  case kind_caps:                                                        \
    return Callable(                                                     \
        BUILTIN_CODE(                                                    \
            isolate,                                                     \
            ArraySingleArgumentConstructor_##kind_camel##_##mode_camel), \
        ArraySingleArgumentConstructorDescriptor{})
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
Callable CodeFactory::InternalArrayNoArgumentConstructor(Isolate* isolate,
                                                         ElementsKind kind) {
  switch (kind) {
    case PACKED_ELEMENTS:
      return Callable(
          BUILTIN_CODE(isolate, InternalArrayNoArgumentConstructor_Packed),
          ArrayNoArgumentConstructorDescriptor{});
    case HOLEY_ELEMENTS:
      return Callable(
          BUILTIN_CODE(isolate, InternalArrayNoArgumentConstructor_Holey),
          ArrayNoArgumentConstructorDescriptor{});
    default:
      UNREACHABLE();
  }
}

// static
Callable CodeFactory::InternalArraySingleArgumentConstructor(
    Isolate* isolate, ElementsKind kind) {
  switch (kind) {
    case PACKED_ELEMENTS:
      return Callable(
          BUILTIN_CODE(isolate, InternalArraySingleArgumentConstructor_Packed),
          ArraySingleArgumentConstructorDescriptor{});
    case HOLEY_ELEMENTS:
      return Callable(
          BUILTIN_CODE(isolate, InternalArraySingleArgumentConstructor_Holey),
          ArraySingleArgumentConstructorDescriptor{});
    default:
      UNREACHABLE();
  }
}

}  // namespace internal
}  // namespace v8
