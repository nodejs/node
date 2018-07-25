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
  return Callable(stub.GetCode(), Descriptor(stub.isolate()));
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
  CallApiGetterStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::CallApiCallback(Isolate* isolate, int argc) {
  CallApiCallbackStub stub(isolate, argc);
  return make_callable(stub);
}

// static
Callable CodeFactory::LoadGlobalIC(Isolate* isolate, TypeofMode typeof_mode) {
  return Callable(
      typeof_mode == NOT_INSIDE_TYPEOF
          ? BUILTIN_CODE(isolate, LoadGlobalICTrampoline)
          : BUILTIN_CODE(isolate, LoadGlobalICInsideTypeofTrampoline),
      LoadGlobalDescriptor(isolate));
}

// static
Callable CodeFactory::LoadGlobalICInOptimizedCode(Isolate* isolate,
                                                  TypeofMode typeof_mode) {
  return Callable(typeof_mode == NOT_INSIDE_TYPEOF
                      ? BUILTIN_CODE(isolate, LoadGlobalIC)
                      : BUILTIN_CODE(isolate, LoadGlobalICInsideTypeof),
                  LoadGlobalWithVectorDescriptor(isolate));
}

Callable CodeFactory::StoreOwnIC(Isolate* isolate) {
  // TODO(ishell): Currently we use StoreOwnIC only for storing properties that
  // already exist in the boilerplate therefore we can use StoreIC.
  return Callable(BUILTIN_CODE(isolate, StoreICTrampoline),
                  StoreDescriptor(isolate));
}

Callable CodeFactory::StoreOwnICInOptimizedCode(Isolate* isolate) {
  // TODO(ishell): Currently we use StoreOwnIC only for storing properties that
  // already exist in the boilerplate therefore we can use StoreIC.
  return Callable(BUILTIN_CODE(isolate, StoreIC),
                  StoreWithVectorDescriptor(isolate));
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
                  TypeConversionDescriptor(isolate));
}

// static
Callable CodeFactory::OrdinaryToPrimitive(Isolate* isolate,
                                          OrdinaryToPrimitiveHint hint) {
  return Callable(isolate->builtins()->OrdinaryToPrimitive(hint),
                  TypeConversionDescriptor(isolate));
}

// static
Callable CodeFactory::StringAdd(Isolate* isolate, StringAddFlags flags,
                                PretenureFlag pretenure_flag) {
  if (pretenure_flag == NOT_TENURED) {
    switch (flags) {
      case STRING_ADD_CHECK_NONE:
        return Builtins::CallableFor(isolate,
                                     Builtins::kStringAdd_CheckNone_NotTenured);
      case STRING_ADD_CONVERT_LEFT:
        return Builtins::CallableFor(
            isolate, Builtins::kStringAdd_ConvertLeft_NotTenured);
      case STRING_ADD_CONVERT_RIGHT:
        return Builtins::CallableFor(
            isolate, Builtins::kStringAdd_ConvertRight_NotTenured);
    }
  } else {
    CHECK_EQ(TENURED, pretenure_flag);
    CHECK_EQ(STRING_ADD_CHECK_NONE, flags);
    return Builtins::CallableFor(isolate,
                                 Builtins::kStringAdd_CheckNone_Tenured);
  }

  UNREACHABLE();
}

// static
Callable CodeFactory::ResumeGenerator(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ResumeGeneratorTrampoline),
                  ResumeGeneratorDescriptor(isolate));
}

// static
Callable CodeFactory::FrameDropperTrampoline(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, FrameDropperTrampoline),
                  FrameDropperTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::HandleDebuggerStatement(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, HandleDebuggerStatement),
                  ContextOnlyDescriptor(isolate));
}

// static
Callable CodeFactory::FastNewFunctionContext(Isolate* isolate,
                                             ScopeType scope_type) {
  return Callable(isolate->builtins()->NewFunctionContext(scope_type),
                  FastNewFunctionContextDescriptor(isolate));
}

// static
Callable CodeFactory::ArgumentAdaptor(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ArgumentsAdaptorTrampoline),
                  ArgumentAdaptorDescriptor(isolate));
}

// static
Callable CodeFactory::Call(Isolate* isolate, ConvertReceiverMode mode) {
  return Callable(isolate->builtins()->Call(mode),
                  CallTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::CallWithArrayLike(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, CallWithArrayLike),
                  CallWithArrayLikeDescriptor(isolate));
}

// static
Callable CodeFactory::CallWithSpread(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, CallWithSpread),
                  CallWithSpreadDescriptor(isolate));
}

// static
Callable CodeFactory::CallFunction(Isolate* isolate, ConvertReceiverMode mode) {
  return Callable(isolate->builtins()->CallFunction(mode),
                  CallTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::CallVarargs(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, CallVarargs),
                  CallVarargsDescriptor(isolate));
}

// static
Callable CodeFactory::CallForwardVarargs(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, CallForwardVarargs),
                  CallForwardVarargsDescriptor(isolate));
}

// static
Callable CodeFactory::CallFunctionForwardVarargs(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, CallFunctionForwardVarargs),
                  CallForwardVarargsDescriptor(isolate));
}

// static
Callable CodeFactory::Construct(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, Construct),
                  ConstructTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::ConstructWithSpread(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ConstructWithSpread),
                  ConstructWithSpreadDescriptor(isolate));
}

// static
Callable CodeFactory::ConstructFunction(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ConstructFunction),
                  ConstructTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::ConstructVarargs(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ConstructVarargs),
                  ConstructVarargsDescriptor(isolate));
}

// static
Callable CodeFactory::ConstructForwardVarargs(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ConstructForwardVarargs),
                  ConstructForwardVarargsDescriptor(isolate));
}

// static
Callable CodeFactory::ConstructFunctionForwardVarargs(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ConstructFunctionForwardVarargs),
                  ConstructForwardVarargsDescriptor(isolate));
}

// static
Callable CodeFactory::InterpreterPushArgsThenCall(
    Isolate* isolate, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  return Callable(
      isolate->builtins()->InterpreterPushArgsThenCall(receiver_mode, mode),
      InterpreterPushArgsThenCallDescriptor(isolate));
}

// static
Callable CodeFactory::InterpreterPushArgsThenConstruct(
    Isolate* isolate, InterpreterPushArgsMode mode) {
  return Callable(isolate->builtins()->InterpreterPushArgsThenConstruct(mode),
                  InterpreterPushArgsThenConstructDescriptor(isolate));
}

// static
Callable CodeFactory::InterpreterCEntry(Isolate* isolate, int result_size) {
  // Note: If we ever use fpregs in the interpreter then we will need to
  // save fpregs too.
  Handle<Code> code = CodeFactory::CEntry(isolate, result_size, kDontSaveFPRegs,
                                          kArgvInRegister);
  return Callable(code, InterpreterCEntryDescriptor(isolate));
}

// static
Callable CodeFactory::InterpreterOnStackReplacement(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, InterpreterOnStackReplacement),
                  ContextOnlyDescriptor(isolate));
}

// static
Callable CodeFactory::ArrayConstructor(Isolate* isolate) {
  ArrayConstructorStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::ArrayPop(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ArrayPop), BuiltinDescriptor(isolate));
}

// static
Callable CodeFactory::ArrayShift(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ArrayShift),
                  BuiltinDescriptor(isolate));
}

// static
Callable CodeFactory::ExtractFastJSArray(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ExtractFastJSArray),
                  ExtractFastJSArrayDescriptor(isolate));
}

// static
Callable CodeFactory::CloneFastJSArray(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, CloneFastJSArray),
                  CloneFastJSArrayDescriptor(isolate));
}

// static
Callable CodeFactory::ArrayPush(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, ArrayPush), BuiltinDescriptor(isolate));
}

// static
Callable CodeFactory::FunctionPrototypeBind(Isolate* isolate) {
  return Callable(BUILTIN_CODE(isolate, FunctionPrototypeBind),
                  BuiltinDescriptor(isolate));
}

// static
Callable CodeFactory::TransitionElementsKind(Isolate* isolate,
                                             ElementsKind from, ElementsKind to,
                                             bool is_jsarray) {
  TransitionElementsKindStub stub(isolate, from, to, is_jsarray);
  return make_callable(stub);
}

}  // namespace internal
}  // namespace v8
