// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_INTERPRETER_H_
#define V8_INTERPRETER_INTERPRETER_H_

// Clients of this interface shouldn't depend on lots of interpreter internals.
// Do not include anything from src/interpreter other than
// src/interpreter/bytecodes.h here!
#include "src/base/macros.h"
#include "src/builtins.h"
#include "src/interpreter/bytecodes.h"
#include "src/parsing/token.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

class Isolate;
class Callable;
class CompilationInfo;

namespace interpreter {

class InterpreterAssembler;

class Interpreter {
 public:
  explicit Interpreter(Isolate* isolate);
  virtual ~Interpreter() {}

  // Initializes the interpreter dispatch table.
  void Initialize();

  // Returns the interrupt budget which should be used for the profiler counter.
  static int InterruptBudget();

  // Generate bytecode for |info|.
  static bool MakeBytecode(CompilationInfo* info);

  // Return bytecode handler for |bytecode|.
  Code* GetBytecodeHandler(Bytecode bytecode);

  // GC support.
  void IterateDispatchTable(ObjectVisitor* v);

  void TraceCodegen(Handle<Code> code, const char* name);

  Address dispatch_table_address() {
    return reinterpret_cast<Address>(&dispatch_table_[0]);
  }

 private:
// Bytecode handler generator functions.
#define DECLARE_BYTECODE_HANDLER_GENERATOR(Name, ...) \
  void Do##Name(InterpreterAssembler* assembler);
  BYTECODE_LIST(DECLARE_BYTECODE_HANDLER_GENERATOR)
#undef DECLARE_BYTECODE_HANDLER_GENERATOR

  // Generates code to perform the binary operations via |function_id|.
  void DoBinaryOp(Runtime::FunctionId function_id,
                  InterpreterAssembler* assembler);

  // Generates code to perform the count operations via |function_id|.
  void DoCountOp(Runtime::FunctionId function_id,
                 InterpreterAssembler* assembler);

  // Generates code to perform the comparison operation associated with
  // |compare_op|.
  void DoCompareOp(Token::Value compare_op, InterpreterAssembler* assembler);

  // Generates code to load a constant from the constant pool.
  void DoLoadConstant(InterpreterAssembler* assembler);

  // Generates code to perform a global load via |ic|.
  void DoLoadGlobal(Callable ic, InterpreterAssembler* assembler);

  // Generates code to perform a global store via |ic|.
  void DoStoreGlobal(Callable ic, InterpreterAssembler* assembler);

  // Generates code to perform a named property load via |ic|.
  void DoLoadIC(Callable ic, InterpreterAssembler* assembler);

  // Generates code to perform a keyed property load via |ic|.
  void DoKeyedLoadIC(Callable ic, InterpreterAssembler* assembler);

  // Generates code to perform a namedproperty store via |ic|.
  void DoStoreIC(Callable ic, InterpreterAssembler* assembler);

  // Generates code to perform a keyed property store via |ic|.
  void DoKeyedStoreIC(Callable ic, InterpreterAssembler* assembler);

  // Generates code to perform a JS call.
  void DoJSCall(InterpreterAssembler* assembler, TailCallMode tail_call_mode);

  // Generates code to perform a runtime call.
  void DoCallRuntimeCommon(InterpreterAssembler* assembler);

  // Generates code to perform a runtime call returning a pair.
  void DoCallRuntimeForPairCommon(InterpreterAssembler* assembler);

  // Generates code to perform a JS runtime call.
  void DoCallJSRuntimeCommon(InterpreterAssembler* assembler);

  // Generates code to perform a constructor call..
  void DoCallConstruct(InterpreterAssembler* assembler);

  // Generates code ro create a literal via |function_id|.
  void DoCreateLiteral(Runtime::FunctionId function_id,
                       InterpreterAssembler* assembler);

  // Generates code to perform delete via function_id.
  void DoDelete(Runtime::FunctionId function_id,
                InterpreterAssembler* assembler);

  // Generates code to perform a lookup slot load via |function_id|.
  void DoLoadLookupSlot(Runtime::FunctionId function_id,
                        InterpreterAssembler* assembler);

  // Generates code to perform a lookup slot store depending on |language_mode|.
  void DoStoreLookupSlot(LanguageMode language_mode,
                         InterpreterAssembler* assembler);

  bool IsDispatchTableInitialized();

  static const int kDispatchTableSize = static_cast<int>(Bytecode::kLast) + 1;

  Isolate* isolate_;
  Code* dispatch_table_[kDispatchTableSize];

  DISALLOW_COPY_AND_ASSIGN(Interpreter);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_INTERPRETER_H_
