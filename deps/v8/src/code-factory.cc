// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/bootstrapper.h"
#include "src/code-factory.h"
#include "src/ic/ic.h"

namespace v8 {
namespace internal {


// static
Callable CodeFactory::LoadIC(Isolate* isolate, ContextualMode mode,
                             LanguageMode language_mode) {
  return Callable(
      LoadIC::initialize_stub(
          isolate, LoadICState(mode, language_mode).GetExtraICState()),
      LoadDescriptor(isolate));
}


// static
Callable CodeFactory::LoadICInOptimizedCode(
    Isolate* isolate, ContextualMode mode, LanguageMode language_mode,
    InlineCacheState initialization_state) {
  auto code = LoadIC::initialize_stub_in_optimized_code(
      isolate, LoadICState(mode, language_mode).GetExtraICState(),
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
                             CallICState::CallType call_type) {
  return Callable(CallIC::initialize_stub(isolate, argc, call_type),
                  CallFunctionWithFeedbackDescriptor(isolate));
}


// static
Callable CodeFactory::CallICInOptimizedCode(Isolate* isolate, int argc,
                                            CallICState::CallType call_type) {
  return Callable(
      CallIC::initialize_stub_in_optimized_code(isolate, argc, call_type),
      CallFunctionWithFeedbackAndVectorDescriptor(isolate));
}


// static
Callable CodeFactory::StoreIC(Isolate* isolate, LanguageMode language_mode) {
  return Callable(
      StoreIC::initialize_stub(isolate, language_mode, UNINITIALIZED),
      FLAG_vector_stores ? VectorStoreICTrampolineDescriptor(isolate)
                         : StoreDescriptor(isolate));
}


// static
Callable CodeFactory::StoreICInOptimizedCode(
    Isolate* isolate, LanguageMode language_mode,
    InlineCacheState initialization_state) {
  CallInterfaceDescriptor descriptor =
      FLAG_vector_stores && initialization_state != MEGAMORPHIC
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
      FLAG_vector_stores ? VectorStoreICTrampolineDescriptor(isolate)
                         : StoreDescriptor(isolate));
}


// static
Callable CodeFactory::KeyedStoreICInOptimizedCode(
    Isolate* isolate, LanguageMode language_mode,
    InlineCacheState initialization_state) {
  CallInterfaceDescriptor descriptor =
      FLAG_vector_stores && initialization_state != MEGAMORPHIC
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
Callable CodeFactory::BinaryOpIC(Isolate* isolate, Token::Value op,
                                 Strength strength) {
  BinaryOpICStub stub(isolate, op, strength);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::Instanceof(Isolate* isolate,
                                 InstanceofStub::Flags flags) {
  InstanceofStub stub(isolate, flags);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::ToBoolean(Isolate* isolate,
                                ToBooleanStub::ResultMode mode,
                                ToBooleanStub::Types types) {
  ToBooleanStub stub(isolate, mode, types);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::ToNumber(Isolate* isolate) {
  ToNumberStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::StringAdd(Isolate* isolate, StringAddFlags flags,
                                PretenureFlag pretenure_flag) {
  StringAddStub stub(isolate, flags, pretenure_flag);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::Typeof(Isolate* isolate) {
  TypeofStub stub(isolate);
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
Callable CodeFactory::FastNewClosure(Isolate* isolate,
                                     LanguageMode language_mode,
                                     FunctionKind kind) {
  FastNewClosureStub stub(isolate, language_mode, kind);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::AllocateHeapNumber(Isolate* isolate) {
  AllocateHeapNumberStub stub(isolate);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}


// static
Callable CodeFactory::CallFunction(Isolate* isolate, int argc,
                                   CallFunctionFlags flags) {
  CallFunctionStub stub(isolate, argc, flags);
  return Callable(stub.GetCode(), stub.GetCallInterfaceDescriptor());
}

}  // namespace internal
}  // namespace v8
