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
class Callable FINAL BASE_EMBEDDED {
 public:
  Callable(Handle<Code> code, CallInterfaceDescriptor descriptor)
      : code_(code), descriptor_(descriptor) {}

  Handle<Code> code() const { return code_; }
  CallInterfaceDescriptor descriptor() const { return descriptor_; }

 private:
  const Handle<Code> code_;
  const CallInterfaceDescriptor descriptor_;
};


class CodeFactory FINAL {
 public:
  // Initial states for ICs.
  static Callable LoadIC(Isolate* isolate, ContextualMode mode);
  static Callable LoadICInOptimizedCode(Isolate* isolate, ContextualMode mode);
  static Callable KeyedLoadIC(Isolate* isolate);
  static Callable KeyedLoadICInOptimizedCode(Isolate* isolate);
  static Callable CallIC(Isolate* isolate, int argc,
                         CallICState::CallType call_type);
  static Callable CallICInOptimizedCode(Isolate* isolate, int argc,
                                        CallICState::CallType call_type);
  static Callable StoreIC(Isolate* isolate, LanguageMode mode);
  static Callable KeyedStoreIC(Isolate* isolate, LanguageMode mode);

  static Callable CompareIC(Isolate* isolate, Token::Value op);

  static Callable BinaryOpIC(Isolate* isolate, Token::Value op);

  // Code stubs. Add methods here as needed to reduce dependency on
  // code-stubs.h.
  static Callable ToBoolean(
      Isolate* isolate, ToBooleanStub::ResultMode mode,
      ToBooleanStub::Types types = ToBooleanStub::Types());

  static Callable ToNumber(Isolate* isolate);

  static Callable StringAdd(Isolate* isolate, StringAddFlags flags,
                            PretenureFlag pretenure_flag);

  static Callable AllocateHeapNumber(Isolate* isolate);

  static Callable CallFunction(Isolate* isolate, int argc,
                               CallFunctionFlags flags);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODE_FACTORY_H_
