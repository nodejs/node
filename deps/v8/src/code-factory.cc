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
  CEntryStub stub(isolate, result_size);
  return stub.GetCode();
}

// static
Callable CodeFactory::LoadICProtoArray(Isolate* isolate,
                                       bool throw_if_nonexistent) {
  return Callable(
      throw_if_nonexistent
          ? isolate->builtins()->LoadICProtoArrayThrowIfNonexistent()
          : isolate->builtins()->LoadICProtoArray(),
      LoadICProtoArrayDescriptor(isolate));
}

// static
Callable CodeFactory::ApiGetter(Isolate* isolate) {
  CallApiGetterStub stub(isolate);
  return make_callable(stub);
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
Callable CodeFactory::CallIC(Isolate* isolate, ConvertReceiverMode mode) {
  CallICStub stub(isolate, mode);
  return make_callable(stub);
}

// static
Callable CodeFactory::CallICTrampoline(Isolate* isolate,
                                       ConvertReceiverMode mode) {
  CallICTrampolineStub stub(isolate, mode);
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
Callable CodeFactory::StoreIC_Uninitialized(Isolate* isolate,
                                            LanguageMode language_mode) {
  return Callable(language_mode == STRICT
                      ? isolate->builtins()->StoreICStrict_Uninitialized()
                      : isolate->builtins()->StoreIC_Uninitialized(),
                  StoreWithVectorDescriptor(isolate));
}

Callable CodeFactory::StoreOwnIC(Isolate* isolate) {
  // TODO(ishell): Currently we use StoreOwnIC only for storing properties that
  // already exist in the boilerplate therefore we can use StoreIC.
  return Callable(isolate->builtins()->StoreICStrictTrampoline(),
                  StoreDescriptor(isolate));
}

Callable CodeFactory::StoreOwnICInOptimizedCode(Isolate* isolate) {
  // TODO(ishell): Currently we use StoreOwnIC only for storing properties that
  // already exist in the boilerplate therefore we can use StoreIC.
  return Callable(isolate->builtins()->StoreICStrict(),
                  StoreWithVectorDescriptor(isolate));
}

// static
Callable CodeFactory::StoreGlobalIC(Isolate* isolate,
                                    LanguageMode language_mode) {
  // TODO(ishell): Use StoreGlobalIC[Strict]Trampoline when it's ready.
  return Callable(language_mode == STRICT
                      ? isolate->builtins()->StoreICStrictTrampoline()
                      : isolate->builtins()->StoreICTrampoline(),
                  StoreDescriptor(isolate));
}

// static
Callable CodeFactory::StoreGlobalICInOptimizedCode(Isolate* isolate,
                                                   LanguageMode language_mode) {
  // TODO(ishell): Use StoreGlobalIC[Strict] when it's ready.
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
  return Callable(language_mode == STRICT
                      ? isolate->builtins()->KeyedStoreIC_Megamorphic_Strict()
                      : isolate->builtins()->KeyedStoreIC_Megamorphic(),
                  StoreWithVectorDescriptor(isolate));
}

// static
Callable CodeFactory::CompareIC(Isolate* isolate, Token::Value op) {
  CompareICStub stub(isolate, op);
  return make_callable(stub);
}

// static
Callable CodeFactory::BinaryOperation(Isolate* isolate, Token::Value op) {
  switch (op) {
    case Token::SAR:
      return Builtins::CallableFor(isolate, Builtins::kShiftRight);
    case Token::SHL:
      return Builtins::CallableFor(isolate, Builtins::kShiftLeft);
    case Token::SHR:
      return Builtins::CallableFor(isolate, Builtins::kShiftRightLogical);
    case Token::ADD:
      return Builtins::CallableFor(isolate, Builtins::kAdd);
    case Token::SUB:
      return Builtins::CallableFor(isolate, Builtins::kSubtract);
    case Token::MUL:
      return Builtins::CallableFor(isolate, Builtins::kMultiply);
    case Token::DIV:
      return Builtins::CallableFor(isolate, Builtins::kDivide);
    case Token::MOD:
      return Builtins::CallableFor(isolate, Builtins::kModulus);
    case Token::BIT_OR:
      return Builtins::CallableFor(isolate, Builtins::kBitwiseOr);
    case Token::BIT_AND:
      return Builtins::CallableFor(isolate, Builtins::kBitwiseAnd);
    case Token::BIT_XOR:
      return Builtins::CallableFor(isolate, Builtins::kBitwiseXor);
    default:
      break;
  }
  UNREACHABLE();
}

// static
Callable CodeFactory::GetProperty(Isolate* isolate) {
  GetPropertyStub stub(isolate);
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
      return Builtins::CallableFor(isolate, Builtins::kStringEqual);
    case Token::LT:
      return Builtins::CallableFor(isolate, Builtins::kStringLessThan);
    case Token::GT:
      return Builtins::CallableFor(isolate, Builtins::kStringGreaterThan);
    case Token::LTE:
      return Builtins::CallableFor(isolate, Builtins::kStringLessThanOrEqual);
    case Token::GTE:
      return Builtins::CallableFor(isolate,
                                   Builtins::kStringGreaterThanOrEqual);
    default:
      break;
  }
  UNREACHABLE();
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
Callable CodeFactory::FrameDropperTrampoline(Isolate* isolate) {
  return Callable(isolate->builtins()->FrameDropperTrampoline(),
                  FrameDropperTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::HandleDebuggerStatement(Isolate* isolate) {
  return Callable(isolate->builtins()->HandleDebuggerStatement(),
                  ContextOnlyDescriptor(isolate));
}

// static
Callable CodeFactory::FastCloneShallowArray(
    Isolate* isolate, AllocationSiteMode allocation_mode) {
  return Callable(isolate->builtins()->NewCloneShallowArray(allocation_mode),
                  FastCloneShallowArrayDescriptor(isolate));
}

// static
Callable CodeFactory::FastNewFunctionContext(Isolate* isolate,
                                             ScopeType scope_type) {
  return Callable(isolate->builtins()->NewFunctionContext(scope_type),
                  FastNewFunctionContextDescriptor(isolate));
}

// static
Callable CodeFactory::AllocateHeapNumber(Isolate* isolate) {
  AllocateHeapNumberStub stub(isolate);
  return make_callable(stub);
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
Callable CodeFactory::CallWithArrayLike(Isolate* isolate) {
  return Callable(isolate->builtins()->CallWithArrayLike(),
                  CallWithArrayLikeDescriptor(isolate));
}

// static
Callable CodeFactory::CallWithSpread(Isolate* isolate) {
  return Callable(isolate->builtins()->CallWithSpread(),
                  CallWithSpreadDescriptor(isolate));
}

// static
Callable CodeFactory::CallFunction(Isolate* isolate, ConvertReceiverMode mode) {
  return Callable(isolate->builtins()->CallFunction(mode),
                  CallTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::CallVarargs(Isolate* isolate) {
  return Callable(isolate->builtins()->CallVarargs(),
                  CallVarargsDescriptor(isolate));
}

// static
Callable CodeFactory::CallForwardVarargs(Isolate* isolate) {
  return Callable(isolate->builtins()->CallForwardVarargs(),
                  CallForwardVarargsDescriptor(isolate));
}

// static
Callable CodeFactory::CallFunctionForwardVarargs(Isolate* isolate) {
  return Callable(isolate->builtins()->CallFunctionForwardVarargs(),
                  CallForwardVarargsDescriptor(isolate));
}

// static
Callable CodeFactory::Construct(Isolate* isolate) {
  return Callable(isolate->builtins()->Construct(),
                  ConstructTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::ConstructWithSpread(Isolate* isolate) {
  return Callable(isolate->builtins()->ConstructWithSpread(),
                  ConstructWithSpreadDescriptor(isolate));
}

// static
Callable CodeFactory::ConstructFunction(Isolate* isolate) {
  return Callable(isolate->builtins()->ConstructFunction(),
                  ConstructTrampolineDescriptor(isolate));
}

// static
Callable CodeFactory::ConstructVarargs(Isolate* isolate) {
  return Callable(isolate->builtins()->ConstructVarargs(),
                  ConstructVarargsDescriptor(isolate));
}

// static
Callable CodeFactory::ConstructForwardVarargs(Isolate* isolate) {
  return Callable(isolate->builtins()->ConstructForwardVarargs(),
                  ConstructForwardVarargsDescriptor(isolate));
}

// static
Callable CodeFactory::ConstructFunctionForwardVarargs(Isolate* isolate) {
  return Callable(isolate->builtins()->ConstructFunctionForwardVarargs(),
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
Callable CodeFactory::InterpreterPushArgsThenConstructArray(Isolate* isolate) {
  return Callable(isolate->builtins()->InterpreterPushArgsThenConstructArray(),
                  InterpreterPushArgsThenConstructArrayDescriptor(isolate));
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
Callable CodeFactory::ArrayConstructor(Isolate* isolate) {
  ArrayConstructorStub stub(isolate);
  return make_callable(stub);
}

// static
Callable CodeFactory::ArrayPop(Isolate* isolate) {
  return Callable(isolate->builtins()->ArrayPop(), BuiltinDescriptor(isolate));
}

// static
Callable CodeFactory::ArrayShift(Isolate* isolate) {
  return Callable(isolate->builtins()->ArrayShift(),
                  BuiltinDescriptor(isolate));
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

// static
Callable CodeFactory::TransitionElementsKind(Isolate* isolate,
                                             ElementsKind from, ElementsKind to,
                                             bool is_jsarray) {
  TransitionElementsKindStub stub(isolate, from, to, is_jsarray);
  return make_callable(stub);
}

}  // namespace internal
}  // namespace v8
