// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_FACTORY_H_
#define V8_CODE_FACTORY_H_

#include "src/allocation.h"
#include "src/assembler.h"
#include "src/codegen.h"
#include "src/globals.h"
#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

// Associates a body of code with an interface descriptor.
class Callable final BASE_EMBEDDED {
 public:
  Callable(Handle<Code> code, CallInterfaceDescriptor descriptor)
      : code_(code), descriptor_(descriptor) {}

  Handle<Code> code() const { return code_; }
  CallInterfaceDescriptor descriptor() const { return descriptor_; }

 private:
  const Handle<Code> code_;
  const CallInterfaceDescriptor descriptor_;
};

class V8_EXPORT_PRIVATE CodeFactory final {
 public:
  // CEntryStub has var-args semantics (all the arguments are passed on the
  // stack and the arguments count is passed via register) which currently
  // can't be expressed in CallInterfaceDescriptor. Therefore only the code
  // is exported here.
  static Handle<Code> RuntimeCEntry(Isolate* isolate, int result_size = 1);

  // Initial states for ICs.
  static Callable LoadIC(Isolate* isolate);
  static Callable LoadICInOptimizedCode(Isolate* isolate);
  static Callable LoadGlobalIC(Isolate* isolate, TypeofMode typeof_mode);
  static Callable LoadGlobalICInOptimizedCode(Isolate* isolate,
                                              TypeofMode typeof_mode);
  static Callable KeyedLoadIC(Isolate* isolate);
  static Callable KeyedLoadICInOptimizedCode(Isolate* isolate);
  static Callable KeyedLoadIC_Megamorphic(Isolate* isolate);
  static Callable CallIC(Isolate* isolate,
                         ConvertReceiverMode mode = ConvertReceiverMode::kAny,
                         TailCallMode tail_call_mode = TailCallMode::kDisallow);
  static Callable CallICInOptimizedCode(
      Isolate* isolate, ConvertReceiverMode mode = ConvertReceiverMode::kAny,
      TailCallMode tail_call_mode = TailCallMode::kDisallow);
  static Callable StoreIC(Isolate* isolate, LanguageMode mode);
  static Callable StoreICInOptimizedCode(Isolate* isolate, LanguageMode mode);
  static Callable KeyedStoreIC(Isolate* isolate, LanguageMode mode);
  static Callable KeyedStoreICInOptimizedCode(Isolate* isolate,
                                              LanguageMode mode);
  static Callable KeyedStoreIC_Megamorphic(Isolate* isolate, LanguageMode mode);

  static Callable ResumeGenerator(Isolate* isolate);

  static Callable CompareIC(Isolate* isolate, Token::Value op);
  static Callable CompareNilIC(Isolate* isolate, NilValue nil_value);

  static Callable BinaryOpIC(Isolate* isolate, Token::Value op);

  static Callable ApiGetter(Isolate* isolate);

  // Code stubs. Add methods here as needed to reduce dependency on
  // code-stubs.h.
  static Callable InstanceOf(Isolate* isolate);
  static Callable OrdinaryHasInstance(Isolate* isolate);

  static Callable StringFromCharCode(Isolate* isolate);

  static Callable GetProperty(Isolate* isolate);

  static Callable ToBoolean(Isolate* isolate);

  static Callable ToNumber(Isolate* isolate);
  static Callable NonNumberToNumber(Isolate* isolate);
  static Callable StringToNumber(Isolate* isolate);
  static Callable ToString(Isolate* isolate);
  static Callable ToName(Isolate* isolate);
  static Callable ToInteger(Isolate* isolate);
  static Callable ToLength(Isolate* isolate);
  static Callable ToObject(Isolate* isolate);
  static Callable NonPrimitiveToPrimitive(
      Isolate* isolate, ToPrimitiveHint hint = ToPrimitiveHint::kDefault);
  static Callable OrdinaryToPrimitive(Isolate* isolate,
                                      OrdinaryToPrimitiveHint hint);
  static Callable NumberToString(Isolate* isolate);

  static Callable RegExpExec(Isolate* isolate);

  static Callable Add(Isolate* isolate);
  static Callable Subtract(Isolate* isolate);
  static Callable Multiply(Isolate* isolate);
  static Callable Divide(Isolate* isolate);
  static Callable Modulus(Isolate* isolate);
  static Callable ShiftRight(Isolate* isolate);
  static Callable ShiftRightLogical(Isolate* isolate);
  static Callable ShiftLeft(Isolate* isolate);
  static Callable BitwiseAnd(Isolate* isolate);
  static Callable BitwiseOr(Isolate* isolate);
  static Callable BitwiseXor(Isolate* isolate);
  static Callable LessThan(Isolate* isolate);
  static Callable LessThanOrEqual(Isolate* isolate);
  static Callable GreaterThan(Isolate* isolate);
  static Callable GreaterThanOrEqual(Isolate* isolate);
  static Callable Equal(Isolate* isolate);
  static Callable NotEqual(Isolate* isolate);
  static Callable StrictEqual(Isolate* isolate);
  static Callable StrictNotEqual(Isolate* isolate);

  static Callable StringAdd(Isolate* isolate, StringAddFlags flags,
                            PretenureFlag pretenure_flag);
  static Callable StringCharAt(Isolate* isolate);
  static Callable StringCharCodeAt(Isolate* isolate);
  static Callable StringCompare(Isolate* isolate, Token::Value token);
  static Callable StringEqual(Isolate* isolate);
  static Callable StringNotEqual(Isolate* isolate);
  static Callable StringLessThan(Isolate* isolate);
  static Callable StringLessThanOrEqual(Isolate* isolate);
  static Callable StringGreaterThan(Isolate* isolate);
  static Callable StringGreaterThanOrEqual(Isolate* isolate);
  static Callable SubString(Isolate* isolate);

  static Callable Typeof(Isolate* isolate);
  static Callable GetSuperConstructor(Isolate* isolate);

  static Callable FastCloneRegExp(Isolate* isolate);
  static Callable FastCloneShallowArray(Isolate* isolate,
                                        AllocationSiteMode allocation_mode);
  static Callable FastCloneShallowObject(Isolate* isolate, int length);

  static Callable FastNewFunctionContext(Isolate* isolate,
                                         ScopeType scope_type);
  static Callable FastNewClosure(Isolate* isolate);
  static Callable FastNewObject(Isolate* isolate);
  static Callable FastNewRestParameter(Isolate* isolate,
                                       bool skip_stub_frame = false);
  static Callable FastNewSloppyArguments(Isolate* isolate,
                                         bool skip_stub_frame = false);
  static Callable FastNewStrictArguments(Isolate* isolate,
                                         bool skip_stub_frame = false);

  static Callable CopyFastSmiOrObjectElements(Isolate* isolate);
  static Callable GrowFastDoubleElements(Isolate* isolate);
  static Callable GrowFastSmiOrObjectElements(Isolate* isolate);

  static Callable NewUnmappedArgumentsElements(Isolate* isolate);
  static Callable NewRestParameterElements(Isolate* isolate);

  static Callable AllocateHeapNumber(Isolate* isolate);
#define SIMD128_ALLOC(TYPE, Type, type, lane_count, lane_type) \
  static Callable Allocate##Type(Isolate* isolate);
  SIMD128_TYPES(SIMD128_ALLOC)
#undef SIMD128_ALLOC

  static Callable ArgumentAdaptor(Isolate* isolate);
  static Callable Call(Isolate* isolate,
                       ConvertReceiverMode mode = ConvertReceiverMode::kAny,
                       TailCallMode tail_call_mode = TailCallMode::kDisallow);
  static Callable CallFunction(
      Isolate* isolate, ConvertReceiverMode mode = ConvertReceiverMode::kAny);
  static Callable Construct(Isolate* isolate);
  static Callable ConstructFunction(Isolate* isolate);
  static Callable CreateIterResultObject(Isolate* isolate);
  static Callable HasProperty(Isolate* isolate);
  static Callable ForInFilter(Isolate* isolate);

  static Callable InterpreterPushArgsAndCall(
      Isolate* isolate, TailCallMode tail_call_mode,
      CallableType function_type = CallableType::kAny);
  static Callable InterpreterPushArgsAndConstruct(
      Isolate* isolate, CallableType function_type = CallableType::kAny);
  static Callable InterpreterPushArgsAndConstructArray(Isolate* isolate);
  static Callable InterpreterCEntry(Isolate* isolate, int result_size = 1);
  static Callable InterpreterOnStackReplacement(Isolate* isolate);

  static Callable ArrayPush(Isolate* isolate);
  static Callable FunctionPrototypeBind(Isolate* isolate);
  static Callable PromiseHandleReject(Isolate* isolate);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODE_FACTORY_H_
