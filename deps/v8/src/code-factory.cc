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
Callable CodeFactory::LoadIC(Isolate* isolate) {
  if (FLAG_tf_load_ic_stub) {
    LoadICTrampolineTFStub stub(isolate);
    return make_callable(stub);
  }
  LoadICTrampolineStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::ApiGetter(Isolate* isolate) {
  CallApiGetterStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::LoadICInOptimizedCode(Isolate* isolate) {
  if (FLAG_tf_load_ic_stub) {
    LoadICTFStub stub(isolate);
    return make_callable(stub);
  }
  LoadICStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::LoadGlobalIC(Isolate* isolate, TypeofMode typeof_mode) {
  LoadGlobalICTrampolineStub stub(isolate, LoadGlobalICState(typeof_mode));
  return make_callable(stub);
}

// static
Callable CodeFactory::LoadGlobalICInOptimizedCode(Isolate* isolate,
                                                  TypeofMode typeof_mode) {
  LoadGlobalICStub stub(isolate, LoadGlobalICState(typeof_mode));
  return make_callable(stub);
}

// static
Callable CodeFactory::KeyedLoadIC(Isolate* isolate) {
  if (FLAG_tf_load_ic_stub) {
    KeyedLoadICTrampolineTFStub stub(isolate);
    return make_callable(stub);
  }
  KeyedLoadICTrampolineStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::KeyedLoadICInOptimizedCode(Isolate* isolate) {
  if (FLAG_tf_load_ic_stub) {
    KeyedLoadICTFStub stub(isolate);
    return make_callable(stub);
  }
  KeyedLoadICStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::KeyedLoadIC_Megamorphic(Isolate* isolate) {
  if (FLAG_tf_load_ic_stub) {
    return Callable(isolate->builtins()->KeyedLoadIC_Megamorphic_TF(),
                    LoadWithVectorDescriptor(isolate));
  }
  return Callable(isolate->builtins()->KeyedLoadIC_Megamorphic(),
                  LoadWithVectorDescriptor(isolate));
}

// static
Callable CodeFactory::CallIC(Isolate* isolate, int argc,
                             ConvertReceiverMode mode,
                             TailCallMode tail_call_mode) {
  CallICTrampolineStub stub(isolate, CallICState(argc, mode, tail_call_mode));
  return make_callable(stub);
}

// static
Callable CodeFactory::CallICInOptimizedCode(Isolate* isolate, int argc,
                                            ConvertReceiverMode mode,
                                            TailCallMode tail_call_mode) {
  CallICStub stub(isolate, CallICState(argc, mode, tail_call_mode));
  return make_callable(stub);
}

// static
Callable CodeFactory::StoreIC(Isolate* isolate, LanguageMode language_mode) {
  if (FLAG_tf_store_ic_stub) {
    StoreICTrampolineTFStub stub(isolate, StoreICState(language_mode));
    return make_callable(stub);
  }
  StoreICTrampolineStub stub(isolate, StoreICState(language_mode));
  return make_callable(stub);
}

// static
Callable CodeFactory::StoreICInOptimizedCode(Isolate* isolate,
                                             LanguageMode language_mode) {
  if (FLAG_tf_store_ic_stub) {
    StoreICTFStub stub(isolate, StoreICState(language_mode));
    return make_callable(stub);
  }
  StoreICStub stub(isolate, StoreICState(language_mode));
  return make_callable(stub);
}

// static
Callable CodeFactory::KeyedStoreIC(Isolate* isolate,
                                   LanguageMode language_mode) {
  KeyedStoreICTrampolineStub stub(isolate, StoreICState(language_mode));
  return make_callable(stub);
}

// static
Callable CodeFactory::KeyedStoreICInOptimizedCode(Isolate* isolate,
                                                  LanguageMode language_mode) {
  KeyedStoreICStub stub(isolate, StoreICState(language_mode));
  return make_callable(stub);
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
Callable CodeFactory::InstanceOf(Isolate* isolate) {
  InstanceOfStub stub(isolate);
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
Callable CodeFactory::ToString(Isolate* isolate) {
  return Callable(isolate->builtins()->ToString(),
                  TypeConversionDescriptor(isolate));
}

// static
Callable CodeFactory::ToName(Isolate* isolate) {
  return Callable(isolate->builtins()->ToName(),
                  TypeConversionDescriptor(isolate));
}

// static
Callable CodeFactory::ToInteger(Isolate* isolate) {
  ToIntegerStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::ToLength(Isolate* isolate) {
  ToLengthStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::ToObject(Isolate* isolate) {
  ToObjectStub stub(isolate);
  return make_callable(stub);
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
Callable CodeFactory::OrdinaryHasInstance(Isolate* isolate) {
  return Callable(isolate->builtins()->OrdinaryHasInstance(),
                  CompareDescriptor(isolate));
}

// static
Callable CodeFactory::RegExpConstructResult(Isolate* isolate) {
  RegExpConstructResultStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::RegExpExec(Isolate* isolate) {
  RegExpExecStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}

// static
Callable CodeFactory::Add(Isolate* isolate) {
  AddStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::Subtract(Isolate* isolate) {
  SubtractStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::Multiply(Isolate* isolate) {
  MultiplyStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::Divide(Isolate* isolate) {
  DivideStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::Modulus(Isolate* isolate) {
  ModulusStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::ShiftRight(Isolate* isolate) {
  ShiftRightStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::ShiftRightLogical(Isolate* isolate) {
  ShiftRightLogicalStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::ShiftLeft(Isolate* isolate) {
  ShiftLeftStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::BitwiseAnd(Isolate* isolate) {
  BitwiseAndStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::BitwiseOr(Isolate* isolate) {
  BitwiseOrStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::BitwiseXor(Isolate* isolate) {
  BitwiseXorStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::Inc(Isolate* isolate) {
  IncStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::Dec(Isolate* isolate) {
  DecStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::LessThan(Isolate* isolate) {
  LessThanStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::LessThanOrEqual(Isolate* isolate) {
  LessThanOrEqualStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::GreaterThan(Isolate* isolate) {
  GreaterThanStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::GreaterThanOrEqual(Isolate* isolate) {
  GreaterThanOrEqualStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::Equal(Isolate* isolate) {
  EqualStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::NotEqual(Isolate* isolate) {
  NotEqualStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::StrictEqual(Isolate* isolate) {
  StrictEqualStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::StrictNotEqual(Isolate* isolate) {
  StrictNotEqualStub stub(isolate);
  return make_callable(stub);
}

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
Callable CodeFactory::Typeof(Isolate* isolate) {
  TypeofStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::FastCloneRegExp(Isolate* isolate) {
  FastCloneRegExpStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::FastCloneShallowArray(Isolate* isolate) {
  // TODO(mstarzinger): Thread through AllocationSiteMode at some point.
  FastCloneShallowArrayStub stub(isolate, DONT_TRACK_ALLOCATION_SITE);
  return make_callable(stub);
}

// static
Callable CodeFactory::FastCloneShallowObject(Isolate* isolate, int length) {
  FastCloneShallowObjectStub stub(isolate, length);
  return make_callable(stub);
}


// static
Callable CodeFactory::FastNewFunctionContext(Isolate* isolate) {
  FastNewFunctionContextStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::FastNewClosure(Isolate* isolate) {
  FastNewClosureStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::FastNewObject(Isolate* isolate) {
  FastNewObjectStub stub(isolate);
  return make_callable(stub);
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
Callable CodeFactory::HasProperty(Isolate* isolate) {
  HasPropertyStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::ForInFilter(Isolate* isolate) {
  ForInFilterStub stub(isolate);
  return make_callable(stub);
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

}  // namespace internal
}  // namespace v8
