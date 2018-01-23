// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_FACTORY_H_
#define V8_CODE_FACTORY_H_

#include "src/allocation.h"
#include "src/assembler.h"
#include "src/callable.h"
#include "src/code-stubs.h"
#include "src/globals.h"
#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE CodeFactory final {
 public:
  // CEntryStub has var-args semantics (all the arguments are passed on the
  // stack and the arguments count is passed via register) which currently
  // can't be expressed in CallInterfaceDescriptor. Therefore only the code
  // is exported here.
  static Handle<Code> RuntimeCEntry(Isolate* isolate, int result_size = 1);

  // Initial states for ICs.
  static Callable LoadGlobalIC(Isolate* isolate, TypeofMode typeof_mode);
  static Callable LoadGlobalICInOptimizedCode(Isolate* isolate,
                                              TypeofMode typeof_mode);
  static Callable StoreOwnIC(Isolate* isolate);
  static Callable StoreOwnICInOptimizedCode(Isolate* isolate);

  static Callable ResumeGenerator(Isolate* isolate);

  static Callable FrameDropperTrampoline(Isolate* isolate);
  static Callable HandleDebuggerStatement(Isolate* isolate);

  static Callable BinaryOperation(Isolate* isolate, Operation op);

  static Callable ApiGetter(Isolate* isolate);
  static Callable CallApiCallback(Isolate* isolate, int argc);

  // Code stubs. Add methods here as needed to reduce dependency on
  // code-stubs.h.
  static Callable GetProperty(Isolate* isolate);

  static Callable NonPrimitiveToPrimitive(
      Isolate* isolate, ToPrimitiveHint hint = ToPrimitiveHint::kDefault);
  static Callable OrdinaryToPrimitive(Isolate* isolate,
                                      OrdinaryToPrimitiveHint hint);

  static Callable StringAdd(Isolate* isolate,
                            StringAddFlags flags = STRING_ADD_CHECK_NONE,
                            PretenureFlag pretenure_flag = NOT_TENURED);

  static Callable FastNewFunctionContext(Isolate* isolate,
                                         ScopeType scope_type);

  static Callable ArgumentAdaptor(Isolate* isolate);
  static Callable Call(Isolate* isolate,
                       ConvertReceiverMode mode = ConvertReceiverMode::kAny);
  static Callable CallWithArrayLike(Isolate* isolate);
  static Callable CallWithSpread(Isolate* isolate);
  static Callable CallFunction(
      Isolate* isolate, ConvertReceiverMode mode = ConvertReceiverMode::kAny);
  static Callable CallVarargs(Isolate* isolate);
  static Callable CallForwardVarargs(Isolate* isolate);
  static Callable CallFunctionForwardVarargs(Isolate* isolate);
  static Callable Construct(Isolate* isolate);
  static Callable ConstructWithSpread(Isolate* isolate);
  static Callable ConstructFunction(Isolate* isolate);
  static Callable ConstructVarargs(Isolate* isolate);
  static Callable ConstructForwardVarargs(Isolate* isolate);
  static Callable ConstructFunctionForwardVarargs(Isolate* isolate);

  static Callable InterpreterPushArgsThenCall(Isolate* isolate,
                                              ConvertReceiverMode receiver_mode,
                                              InterpreterPushArgsMode mode);
  static Callable InterpreterPushArgsThenConstruct(
      Isolate* isolate, InterpreterPushArgsMode mode);
  static Callable InterpreterCEntry(Isolate* isolate, int result_size = 1);
  static Callable InterpreterOnStackReplacement(Isolate* isolate);

  static Callable ArrayConstructor(Isolate* isolate);
  static Callable ArrayPop(Isolate* isolate);
  static Callable ArrayPush(Isolate* isolate);
  static Callable ArrayShift(Isolate* isolate);
  static Callable ExtractFastJSArray(Isolate* isolate);
  static Callable CloneFastJSArray(Isolate* isolate);
  static Callable FunctionPrototypeBind(Isolate* isolate);
  static Callable TransitionElementsKind(Isolate* isolate, ElementsKind from,
                                         ElementsKind to, bool is_jsarray);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODE_FACTORY_H_
