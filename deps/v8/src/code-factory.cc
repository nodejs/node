// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-factory.h"

#include "src/bootstrapper.h"
#include "src/ic/ic.h"

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
  CEntryStub stub(isolate, result_size);
  return stub.GetCode();
}

// static
Callable CodeFactory::LoadIC(Isolate* isolate) {
  return Callable(isolate->builtins()->LoadICTrampoline(),
                  LoadDescriptor(isolate));
}

// static
Callable CodeFactory::ApiGetter(Isolate* isolate) {
  CallApiGetterStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::LoadICInOptimizedCode(Isolate* isolate) {
  return Callable(isolate->builtins()->LoadIC(),
                  LoadWithVectorDescriptor(isolate));
}

// static
Callable CodeFactory::LoadGlobalIC(Isolate* isolate, TypeofMode typeof_mode) {
  return Callable(
      typeof_mode == NOT_INSIDE_TYPEOF
          ? isolate->builtins()->LoadGlobalICTrampoline()
          : isolate->builtins()->LoadGlobalICInsideTypeofTrampoline(),
      LoadGlobalDescriptor(isolate));
}

// static
Callable CodeFactory::LoadGlobalICInOptimizedCode(Isolate* isolate,
                                                  TypeofMode typeof_mode) {
  return Callable(typeof_mode == NOT_INSIDE_TYPEOF
                      ? isolate->builtins()->LoadGlobalIC()
                      : isolate->builtins()->LoadGlobalICInsideTypeof(),
                  LoadGlobalWithVectorDescriptor(isolate));
}

// static
Callable CodeFactory::KeyedLoadIC(Isolate* isolate) {
  return Callable(isolate->builtins()->KeyedLoadICTrampoline(),
                  LoadDescriptor(isolate));
}

// static
Callable CodeFactory::KeyedLoadICInOptimizedCode(Isolate* isolate) {
  return Callable(isolate->builtins()->KeyedLoadIC(),
                  LoadWithVectorDescriptor(isolate));
}

// static
Callable CodeFactory::KeyedLoadIC_Megamorphic(Isolate* isolate) {
  return Callable(isolate->builtins()->KeyedLoadIC_Megamorphic_TF(),
                  LoadWithVectorDescriptor(isolate));
}

// static
Callable CodeFactory::CallIC(Isolate* isolate, ConvertReceiverMode mode,
                             TailCallMode tail_call_mode) {
  CallICTrampolineStub stub(isolate, CallICState(mode, tail_call_mode));
  return make_callable(stub);
}

// static
Callable CodeFactory::CallICInOptimizedCode(Isolate* isolate,
                                            ConvertReceiverMode mode,
                                            TailCallMode tail_call_mode) {
  CallICStub stub(isolate, CallICState(mode, tail_call_mode));
  return make_callable(stub);
}

// static
Callable CodeFactory::StoreIC(Isolate* isolate, LanguageMode language_mode) {
  return Callable(language_mode == STRICT
                      ? isolate->builtins()->StoreICStrictTrampoline()
                      : isolate->builtins()->StoreICTrampoline(),
                  StoreDescriptor(isolate));
}

// static
Callable CodeFactory::StoreICInOptimizedCode(Isolate* isolate,
                                             LanguageMode language_mode) {
  return Callable(language_mode == STRICT ? isolate->builtins()->StoreICStrict()
                                          : isolate->builtins()->StoreIC(),
                  StoreWithVectorDescriptor(isolate));
}

// static
Callable CodeFactory::KeyedStoreIC(Isolate* isolate,
                                   LanguageMode language_mode) {
  return Callable(language_mode == STRICT
                      ? isolate->builtins()->KeyedStoreICStrictTrampoline()
                      : isolate->builtins()->KeyedStoreICTrampoline(),
                  StoreDescriptor(isolate));
}

// static
Callable CodeFactory::KeyedStoreICInOptimizedCode(Isolate* isolate,
                                                  LanguageMode language_mode) {
  return Callable(language_mode == STRICT
                      ? isolate->builtins()->KeyedStoreICStrict()
                      : isolate->builtins()->KeyedStoreIC(),
                  StoreWithVectorDescriptor(isolate));
}

// static
Callable CodeFactory::KeyedStoreIC_Megamorphic(Isolate* isolate,
                                               LanguageMode language_mode) {
  return Callable(
      language_mode == STRICT
          ? isolate->builtins()->KeyedStoreIC_Megamorphic_Strict_TF()
          : isolate->builtins()->KeyedStoreIC_Megamorphic_TF(),
      StoreWithVectorDescriptor(isolate));
}

// static
Callable CodeFactory::CompareIC(Isolate* isolate, Token::Value op) {
  CompareICStub stub(isolate, op);
  return make_callable(stub);
}

// static
Callable CodeFactory::BinaryOpIC(Isolate* isolate, Token::Value op) {
  BinaryOpICStub stub(isolate, op);
  return make_callable(stub);
}

// static
Callable CodeFactory::GetProperty(Isolate* isolate) {
  GetPropertyStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::ToBoolean(Isolate* isolate) {
  return Callable(isolate->builtins()->ToBoolean(),
                  TypeConversionDescriptor(isolate));
}

// static
Callable CodeFactory::ToNumber(Isolate* isolate) {
  return Callable(isolate->builtins()->ToNumber(),
                  TypeConversionDescriptor(isolate));
}

// static
Callable CodeFactory::NonNumberToNumber(Isolate* isolate) {
  return Callable(isolate->builtins()->NonNumberToNumber(),
                  TypeConversionDescriptor(isolate));
}

// static
Callable CodeFactory::StringToNumber(Isolate* isolate) {
  return Callable(isolate->builtins()->StringToNumber(),
                  TypeConversionDescriptor(isolate));
}

// static
Callable CodeFactory::ToName(Isolate* isolate) {
  return Callable(isolate->builtins()->ToName(),
                  TypeConversionDescriptor(isolate));
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
Callable CodeFactory::NumberToString(Isolate* isolate) {
  NumberToStringStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::RegExpExec(Isolate* isolate) {
  RegExpExecStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}

// static
Callable CodeFactory::StringFromCharCode(Isolate* isolate) {
  Handle<Code> code(isolate->builtins()->StringFromCharCode());
  return Callable(code, BuiltinDescriptor(isolate));
}

#define DECLARE_TFS(Name, Kind, Extra, InterfaceDescriptor) \
  typedef InterfaceDescriptor##Descriptor Name##Descriptor;
BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, DECLARE_TFS,
             IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN)
#undef DECLARE_TFS

#define TFS_BUILTIN(Name)                             \
  Callable CodeFactory::Name(Isolate* isolate) {      \
    Handle<Code> code(isolate->builtins()->Name());   \
    return Callable(code, Name##Descriptor(isolate)); \
  }

TFS_BUILTIN(ToString)
TFS_BUILTIN(Add)
TFS_BUILTIN(Subtract)
TFS_BUILTIN(Multiply)
TFS_BUILTIN(Divide)
TFS_BUILTIN(Modulus)
TFS_BUILTIN(BitwiseAnd)
TFS_BUILTIN(BitwiseOr)
TFS_BUILTIN(BitwiseXor)
TFS_BUILTIN(ShiftLeft)
TFS_BUILTIN(ShiftRight)
TFS_BUILTIN(ShiftRightLogical)
TFS_BUILTIN(LessThan)
TFS_BUILTIN(LessThanOrEqual)
TFS_BUILTIN(GreaterThan)
TFS_BUILTIN(GreaterThanOrEqual)
TFS_BUILTIN(Equal)
TFS_BUILTIN(NotEqual)
TFS_BUILTIN(StrictEqual)
TFS_BUILTIN(StrictNotEqual)
TFS_BUILTIN(CreateIterResultObject)
TFS_BUILTIN(HasProperty)
TFS_BUILTIN(ToInteger)
TFS_BUILTIN(ToLength)
TFS_BUILTIN(ToObject)
TFS_BUILTIN(Typeof)
TFS_BUILTIN(InstanceOf)
TFS_BUILTIN(OrdinaryHasInstance)
TFS_BUILTIN(ForInFilter)
TFS_BUILTIN(NewUnmappedArgumentsElements)
TFS_BUILTIN(NewRestParameterElements)
TFS_BUILTIN(PromiseHandleReject)
TFS_BUILTIN(GetSuperConstructor)
TFS_BUILTIN(StringCharAt)
TFS_BUILTIN(StringCharCodeAt)

#undef TFS_BUILTIN

// static
Callable CodeFactory::StringAdd(Isolate* isolate, StringAddFlags flags,
                                PretenureFlag pretenure_flag) {
  StringAddStub stub(isolate, flags, pretenure_flag);
  return make_callable(stub);
}

// static
Callable CodeFactory::StringCompare(Isolate* isolate, Token::Value token) {
  switch (token) {
    case Token::EQ:
    case Token::EQ_STRICT:
      return StringEqual(isolate);
    case Token::NE:
    case Token::NE_STRICT:
      return StringNotEqual(isolate);
    case Token::LT:
      return StringLessThan(isolate);
    case Token::GT:
      return StringGreaterThan(isolate);
    case Token::LTE:
      return StringLessThanOrEqual(isolate);
    case Token::GTE:
      return StringGreaterThanOrEqual(isolate);
    default:
      break;
  }
  UNREACHABLE();
  return StringEqual(isolate);
}

// static
Callable CodeFactory::StringEqual(Isolate* isolate) {
  return Callable(isolate->builtins()->StringEqual(),
                  CompareDescriptor(isolate));
}

// static
Callable CodeFactory::StringNotEqual(Isolate* isolate) {
  return Callable(isolate->builtins()->StringNotEqual(),
                  CompareDescriptor(isolate));
}

// static
Callable CodeFactory::StringLessThan(Isolate* isolate) {
  return Callable(isolate->builtins()->StringLessThan(),
                  CompareDescriptor(isolate));
}

// static
Callable CodeFactory::StringLessThanOrEqual(Isolate* isolate) {
  return Callable(isolate->builtins()->StringLessThanOrEqual(),
                  CompareDescriptor(isolate));
}

// static
Callable CodeFactory::StringGreaterThan(Isolate* isolate) {
  return Callable(isolate->builtins()->StringGreaterThan(),
                  CompareDescriptor(isolate));
}

// static
Callable CodeFactory::StringGreaterThanOrEqual(Isolate* isolate) {
  return Callable(isolate->builtins()->StringGreaterThanOrEqual(),
                  CompareDescriptor(isolate));
}

// static
Callable CodeFactory::SubString(Isolate* isolate) {
  SubStringStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}

// static
Callable CodeFactory::ResumeGenerator(Isolate* isolate) {
  return Callable(isolate->builtins()->ResumeGeneratorTrampoline(),
                  ResumeGeneratorDescriptor(isolate));
}

// static
Callable CodeFactory::FastCloneRegExp(Isolate* isolate) {
  return Callable(isolate->builtins()->FastCloneRegExp(),
                  FastCloneRegExpDescriptor(isolate));
}

// static
Callable CodeFactory::FastCloneShallowArray(
    Isolate* isolate, AllocationSiteMode allocation_mode) {
  return Callable(isolate->builtins()->NewCloneShallowArray(allocation_mode),
                  FastCloneShallowArrayDescriptor(isolate));
}

// static
Callable CodeFactory::FastCloneShallowObject(Isolate* isolate, int length) {
  return Callable(isolate->builtins()->NewCloneShallowObject(length),
                  FastCloneShallowObjectDescriptor(isolate));
}

// static
Callable CodeFactory::FastNewFunctionContext(Isolate* isolate,
                                             ScopeType scope_type) {
  return Callable(isolate->builtins()->NewFunctionContext(scope_type),
                  FastNewFunctionContextDescriptor(isolate));
}

// static
Callable CodeFactory::FastNewClosure(Isolate* isolate) {
  return Callable(isolate->builtins()->FastNewClosure(),
                  FastNewClosureDescriptor(isolate));
}

// static
Callable CodeFactory::FastNewObject(Isolate* isolate) {
  return Callable(isolate->builtins()->FastNewObject(),
                  FastNewObjectDescriptor(isolate));
}

// static
Callable CodeFactory::FastNewRestParameter(Isolate* isolate,
                                           bool skip_stub_frame) {
  FastNewRestParameterStub stub(isolate, skip_stub_frame);
  return make_callable(stub);
}

// static
Callable CodeFactory::FastNewSloppyArguments(Isolate* isolate,
                                             bool skip_stub_frame) {
  FastNewSloppyArgumentsStub stub(isolate, skip_stub_frame);
  return make_callable(stub);
}

// static
Callable CodeFactory::FastNewStrictArguments(Isolate* isolate,
                                             bool skip_stub_frame) {
  FastNewStrictArgumentsStub stub(isolate, skip_stub_frame);
  return make_callable(stub);
}

// static
Callable CodeFactory::CopyFastSmiOrObjectElements(Isolate* isolate) {
  return Callable(isolate->builtins()->CopyFastSmiOrObjectElements(),
                  CopyFastSmiOrObjectElementsDescriptor(isolate));
}

// static
Callable CodeFactory::GrowFastDoubleElements(Isolate* isolate) {
  return Callable(isolate->builtins()->GrowFastDoubleElements(),
                  GrowArrayElementsDescriptor(isolate));
}

// static
Callable CodeFactory::GrowFastSmiOrObjectElements(Isolate* isolate) {
  return Callable(isolate->builtins()->GrowFastSmiOrObjectElements(),
                  GrowArrayElementsDescriptor(isolate));
}

// static
Callable CodeFactory::AllocateHeapNumber(Isolate* isolate) {
  AllocateHeapNumberStub stub(isolate);
  return make_callable(stub);
}

#define SIMD128_ALLOC(TYPE, Type, type, lane_count, lane_type) \
  Callable CodeFactory::Allocate##Type(Isolate* isolate) {     \
    Allocate##Type##Stub stub(isolate);                        \
    return make_callable(stub);                                \
  }
SIMD128_TYPES(SIMD128_ALLOC)
#undef SIMD128_ALLOC

// static
Callable CodeFactory::ArgumentAdaptor(Isolate* isolate) {
  return Callable(isolate->builtins()->ArgumentsAdaptorTrampoline(),
                  ArgumentAdaptorDescriptor(isolate));
}

// static
Callable CodeFactory::Call(Isolate* isolate, ConvertReceiverMode mode,
                           TailCallMode tail_call_mode) {
  return Callable(isolate->builtins()->Call(mode, tail_call_mode),
                  CallTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::CallFunction(Isolate* isolate, ConvertReceiverMode mode) {
  return Callable(isolate->builtins()->CallFunction(mode),
                  CallTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::Construct(Isolate* isolate) {
  return Callable(isolate->builtins()->Construct(),
                  ConstructTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::ConstructFunction(Isolate* isolate) {
  return Callable(isolate->builtins()->ConstructFunction(),
                  ConstructTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::InterpreterPushArgsAndCall(Isolate* isolate,
                                                 TailCallMode tail_call_mode,
                                                 CallableType function_type) {
  return Callable(isolate->builtins()->InterpreterPushArgsAndCall(
                      tail_call_mode, function_type),
                  InterpreterPushArgsAndCallDescriptor(isolate));
}

// static
Callable CodeFactory::InterpreterPushArgsAndConstruct(
    Isolate* isolate, CallableType function_type) {
  return Callable(
      isolate->builtins()->InterpreterPushArgsAndConstruct(function_type),
      InterpreterPushArgsAndConstructDescriptor(isolate));
}

// static
Callable CodeFactory::InterpreterPushArgsAndConstructArray(Isolate* isolate) {
  return Callable(isolate->builtins()->InterpreterPushArgsAndConstructArray(),
                  InterpreterPushArgsAndConstructArrayDescriptor(isolate));
}

// static
Callable CodeFactory::InterpreterCEntry(Isolate* isolate, int result_size) {
  // Note: If we ever use fpregs in the interpreter then we will need to
  // save fpregs too.
  CEntryStub stub(isolate, result_size, kDontSaveFPRegs, kArgvInRegister);
  return Callable(stub.GetCode(), InterpreterCEntryDescriptor(isolate));
}

// static
Callable CodeFactory::InterpreterOnStackReplacement(Isolate* isolate) {
  return Callable(isolate->builtins()->InterpreterOnStackReplacement(),
                  ContextOnlyDescriptor(isolate));
}

// static
Callable CodeFactory::ArrayPush(Isolate* isolate) {
  return Callable(isolate->builtins()->ArrayPush(), BuiltinDescriptor(isolate));
}

// static
Callable CodeFactory::FunctionPrototypeBind(Isolate* isolate) {
  return Callable(isolate->builtins()->FunctionPrototypeBind(),
                  BuiltinDescriptor(isolate));
}

}  // namespace internal
}  // namespace v8
