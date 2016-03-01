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


class CodeFactory final {
 public:
  // Initial states for ICs.
  static Callable LoadIC(Isolate* isolate, TypeofMode typeof_mode,
                         LanguageMode language_mode);
  static Callable LoadICInOptimizedCode(Isolate* isolate,
                                        TypeofMode typeof_mode,
                                        LanguageMode language_mode,
                                        InlineCacheState initialization_state);
  static Callable KeyedLoadIC(Isolate* isolate, LanguageMode language_mode);
  static Callable KeyedLoadICInOptimizedCode(
      Isolate* isolate, LanguageMode language_mode,
      InlineCacheState initialization_state);
  static Callable CallIC(Isolate* isolate, int argc,
                         ConvertReceiverMode mode = ConvertReceiverMode::kAny);
  static Callable CallICInOptimizedCode(
      Isolate* isolate, int argc,
      ConvertReceiverMode mode = ConvertReceiverMode::kAny);
  static Callable StoreIC(Isolate* isolate, LanguageMode mode);
  static Callable StoreICInOptimizedCode(Isolate* isolate, LanguageMode mode,
                                         InlineCacheState initialization_state);
  static Callable KeyedStoreIC(Isolate* isolate, LanguageMode mode);
  static Callable KeyedStoreICInOptimizedCode(
      Isolate* isolate, LanguageMode mode,
      InlineCacheState initialization_state);

  static Callable CompareIC(Isolate* isolate, Token::Value op,
                            Strength strength);
  static Callable CompareNilIC(Isolate* isolate, NilValue nil_value);

  static Callable BinaryOpIC(Isolate* isolate, Token::Value op,
                             Strength strength);

  // Code stubs. Add methods here as needed to reduce dependency on
  // code-stubs.h.
  static Callable InstanceOf(Isolate* isolate);

  static Callable ToBoolean(Isolate* isolate);

  static Callable ToNumber(Isolate* isolate);
  static Callable ToString(Isolate* isolate);
  static Callable ToLength(Isolate* isolate);
  static Callable ToObject(Isolate* isolate);
  static Callable NumberToString(Isolate* isolate);

  static Callable RegExpConstructResult(Isolate* isolate);
  static Callable RegExpExec(Isolate* isolate);

  static Callable StringAdd(Isolate* isolate, StringAddFlags flags,
                            PretenureFlag pretenure_flag);
  static Callable StringCompare(Isolate* isolate);
  static Callable SubString(Isolate* isolate);

  static Callable Typeof(Isolate* isolate);

  static Callable FastCloneRegExp(Isolate* isolate);
  static Callable FastCloneShallowArray(Isolate* isolate);
  static Callable FastCloneShallowObject(Isolate* isolate, int length);

  static Callable FastNewContext(Isolate* isolate, int slot_count);
  static Callable FastNewClosure(Isolate* isolate, LanguageMode language_mode,
                                 FunctionKind kind);

  static Callable ArgumentsAccess(Isolate* isolate, bool is_unmapped_arguments,
                                  bool has_duplicate_parameters);
  static Callable RestArgumentsAccess(Isolate* isolate);

  static Callable AllocateHeapNumber(Isolate* isolate);
  static Callable AllocateMutableHeapNumber(Isolate* isolate);
  static Callable AllocateInNewSpace(Isolate* isolate);

  static Callable ArgumentAdaptor(Isolate* isolate);
  static Callable Call(Isolate* isolate,
                       ConvertReceiverMode mode = ConvertReceiverMode::kAny);
  static Callable CallFunction(
      Isolate* isolate, ConvertReceiverMode mode = ConvertReceiverMode::kAny);
  static Callable Construct(Isolate* isolate);
  static Callable ConstructFunction(Isolate* isolate);

  static Callable InterpreterPushArgsAndCall(Isolate* isolate);
  static Callable InterpreterPushArgsAndConstruct(Isolate* isolate);
  static Callable InterpreterCEntry(Isolate* isolate, int result_size = 1);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODE_FACTORY_H_
