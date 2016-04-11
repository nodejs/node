// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-factory.h"

#include "src/bootstrapper.h"
#include "src/ic/ic.h"

namespace v8 {
namespace internal {


// static
Callable CodeFactory::LoadIC(Isolate* isolate, TypeofMode typeof_mode,
                             LanguageMode language_mode) {
  return Callable(
      LoadIC::initialize_stub(
          isolate, LoadICState(typeof_mode, language_mode).GetExtraICState()),
      LoadDescriptor(isolate));
}


// static
Callable CodeFactory::LoadICInOptimizedCode(
    Isolate* isolate, TypeofMode typeof_mode, LanguageMode language_mode,
    InlineCacheState initialization_state) {
  auto code = LoadIC::initialize_stub_in_optimized_code(
      isolate, LoadICState(typeof_mode, language_mode).GetExtraICState(),
      initialization_state);
  return Callable(code, LoadWithVectorDescriptor(isolate));
}


// static
Callable CodeFactory::KeyedLoadIC(Isolate* isolate,
                                  LanguageMode language_mode) {
  ExtraICState state = is_strong(language_mode) ? LoadICState::kStrongModeState
                                                : kNoExtraICState;
  return Callable(KeyedLoadIC::initialize_stub(isolate, state),
                  LoadDescriptor(isolate));
}


// static
Callable CodeFactory::KeyedLoadICInOptimizedCode(
    Isolate* isolate, LanguageMode language_mode,
    InlineCacheState initialization_state) {
  ExtraICState state = is_strong(language_mode) ? LoadICState::kStrongModeState
                                                : kNoExtraICState;
  auto code = KeyedLoadIC::initialize_stub_in_optimized_code(
      isolate, initialization_state, state);
  if (initialization_state != MEGAMORPHIC) {
    return Callable(code, LoadWithVectorDescriptor(isolate));
  }
  return Callable(code, LoadDescriptor(isolate));
}


// static
Callable CodeFactory::CallIC(Isolate* isolate, int argc,
                             ConvertReceiverMode mode) {
  return Callable(CallIC::initialize_stub(isolate, argc, mode),
                  CallFunctionWithFeedbackDescriptor(isolate));
}


// static
Callable CodeFactory::CallICInOptimizedCode(Isolate* isolate, int argc,
                                            ConvertReceiverMode mode) {
  return Callable(
      CallIC::initialize_stub_in_optimized_code(isolate, argc, mode),
      CallFunctionWithFeedbackAndVectorDescriptor(isolate));
}


// static
Callable CodeFactory::StoreIC(Isolate* isolate, LanguageMode language_mode) {
  return Callable(
      StoreIC::initialize_stub(isolate, language_mode, UNINITIALIZED),
      VectorStoreICTrampolineDescriptor(isolate));
}


// static
Callable CodeFactory::StoreICInOptimizedCode(
    Isolate* isolate, LanguageMode language_mode,
    InlineCacheState initialization_state) {
  CallInterfaceDescriptor descriptor = initialization_state != MEGAMORPHIC
                                           ? VectorStoreICDescriptor(isolate)
                                           : StoreDescriptor(isolate);
  return Callable(StoreIC::initialize_stub_in_optimized_code(
                      isolate, language_mode, initialization_state),
                  descriptor);
}


// static
Callable CodeFactory::KeyedStoreIC(Isolate* isolate,
                                   LanguageMode language_mode) {
  return Callable(
      KeyedStoreIC::initialize_stub(isolate, language_mode, UNINITIALIZED),
      VectorStoreICTrampolineDescriptor(isolate));
}


// static
Callable CodeFactory::KeyedStoreICInOptimizedCode(
    Isolate* isolate, LanguageMode language_mode,
    InlineCacheState initialization_state) {
  CallInterfaceDescriptor descriptor = initialization_state != MEGAMORPHIC
                                           ? VectorStoreICDescriptor(isolate)
                                           : StoreDescriptor(isolate);
  return Callable(KeyedStoreIC::initialize_stub_in_optimized_code(
                      isolate, language_mode, initialization_state),
                  descriptor);
}


// static
Callable CodeFactory::CompareIC(Isolate* isolate, Token::Value op,
                                Strength strength) {
  Handle<Code> code = CompareIC::GetUninitialized(isolate, op, strength);
  return Callable(code, CompareDescriptor(isolate));
}


// static
Callable CodeFactory::CompareNilIC(Isolate* isolate, NilValue nil_value) {
  Handle<Code> code = CompareNilICStub::GetUninitialized(isolate, nil_value);
  return Callable(code, CompareNilDescriptor(isolate));
}


// static
Callable CodeFactory::BinaryOpIC(Isolate* isolate, Token::Value op,
                                 Strength strength) {
  BinaryOpICStub stub(isolate, op, strength);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::InstanceOf(Isolate* isolate) {
  InstanceOfStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::ToBoolean(Isolate* isolate) {
  Handle<Code> code = ToBooleanStub::GetUninitialized(isolate);
  return Callable(code, ToBooleanDescriptor(isolate));
}


// static
Callable CodeFactory::ToNumber(Isolate* isolate) {
  ToNumberStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::ToString(Isolate* isolate) {
  ToStringStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::ToLength(Isolate* isolate) {
  ToLengthStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::ToObject(Isolate* isolate) {
  ToObjectStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::NumberToString(Isolate* isolate) {
  NumberToStringStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::RegExpConstructResult(Isolate* isolate) {
  RegExpConstructResultStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::RegExpExec(Isolate* isolate) {
  RegExpExecStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::StringAdd(Isolate* isolate, StringAddFlags flags,
                                PretenureFlag pretenure_flag) {
  StringAddStub stub(isolate, flags, pretenure_flag);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::StringCompare(Isolate* isolate) {
  StringCompareStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::SubString(Isolate* isolate) {
  SubStringStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::Typeof(Isolate* isolate) {
  TypeofStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::FastCloneRegExp(Isolate* isolate) {
  FastCloneRegExpStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::FastCloneShallowArray(Isolate* isolate) {
  // TODO(mstarzinger): Thread through AllocationSiteMode at some point.
  FastCloneShallowArrayStub stub(isolate, DONT_TRACK_ALLOCATION_SITE);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::FastCloneShallowObject(Isolate* isolate, int length) {
  FastCloneShallowObjectStub stub(isolate, length);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::FastNewContext(Isolate* isolate, int slot_count) {
  FastNewContextStub stub(isolate, slot_count);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::FastNewClosure(Isolate* isolate,
                                     LanguageMode language_mode,
                                     FunctionKind kind) {
  FastNewClosureStub stub(isolate, language_mode, kind);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::ArgumentsAccess(Isolate* isolate,
                                      bool is_unmapped_arguments,
                                      bool has_duplicate_parameters) {
  ArgumentsAccessStub::Type type = ArgumentsAccessStub::ComputeType(
      is_unmapped_arguments, has_duplicate_parameters);
  ArgumentsAccessStub stub(isolate, type);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::RestArgumentsAccess(Isolate* isolate) {
  RestParamAccessStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::AllocateHeapNumber(Isolate* isolate) {
  AllocateHeapNumberStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::AllocateMutableHeapNumber(Isolate* isolate) {
  AllocateMutableHeapNumberStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::AllocateInNewSpace(Isolate* isolate) {
  AllocateInNewSpaceStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::ArgumentAdaptor(Isolate* isolate) {
  return Callable(isolate->builtins()->ArgumentsAdaptorTrampoline(),
                  ArgumentAdaptorDescriptor(isolate));
}


// static
Callable CodeFactory::Call(Isolate* isolate, ConvertReceiverMode mode) {
  return Callable(isolate->builtins()->Call(mode),
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
Callable CodeFactory::InterpreterPushArgsAndCall(Isolate* isolate) {
  return Callable(isolate->builtins()->InterpreterPushArgsAndCall(),
                  InterpreterPushArgsAndCallDescriptor(isolate));
}


// static
Callable CodeFactory::InterpreterPushArgsAndConstruct(Isolate* isolate) {
  return Callable(isolate->builtins()->InterpreterPushArgsAndConstruct(),
                  InterpreterPushArgsAndConstructDescriptor(isolate));
}


// static
Callable CodeFactory::InterpreterCEntry(Isolate* isolate, int result_size) {
  // Note: If we ever use fpregs in the interpreter then we will need to
  // save fpregs too.
  CEntryStub stub(isolate, result_size, kDontSaveFPRegs, kArgvInRegister);
  return Callable(stub.GetCode(), InterpreterCEntryDescriptor(isolate));
}

}  // namespace internal
}  // namespace v8
