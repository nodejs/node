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

namespace compiler {
class InterpreterAssembler;
}

namespace interpreter {

class Interpreter {
 public:
  explicit Interpreter(Isolate* isolate);
  virtual ~Interpreter() {}

  // Creates an uninitialized interpreter handler table, where each handler
  // points to the Illegal builtin.
  static Handle<FixedArray> CreateUninitializedInterpreterTable(
      Isolate* isolate);

  // Initializes the interpreter.
  void Initialize();

  // Generate bytecode for |info|.
  static bool MakeBytecode(CompilationInfo* info);

 private:
// Bytecode handler generator functions.
#define DECLARE_BYTECODE_HANDLER_GENERATOR(Name, ...) \
  void Do##Name(compiler::InterpreterAssembler* assembler);
  BYTECODE_LIST(DECLARE_BYTECODE_HANDLER_GENERATOR)
#undef DECLARE_BYTECODE_HANDLER_GENERATOR

  // Generates code to perform the binary operations via |function_id|.
  void DoBinaryOp(Runtime::FunctionId function_id,
                  compiler::InterpreterAssembler* assembler);

  // Generates code to perform the count operations via |function_id|.
  void DoCountOp(Runtime::FunctionId function_id,
                 compiler::InterpreterAssembler* assembler);

  // Generates code to perform the comparison operation associated with
  // |compare_op|.
  void DoCompareOp(Token::Value compare_op,
                   compiler::InterpreterAssembler* assembler);

  // Generates code to load a constant from the constant pool.
  void DoLoadConstant(compiler::InterpreterAssembler* assembler);

  // Generates code to perform a global load via |ic|.
  void DoLoadGlobal(Callable ic, compiler::InterpreterAssembler* assembler);

  // Generates code to perform a global store via |ic|.
  void DoStoreGlobal(Callable ic, compiler::InterpreterAssembler* assembler);

  // Generates code to perform a named property load via |ic|.
  void DoLoadIC(Callable ic, compiler::InterpreterAssembler* assembler);

  // Generates code to perform a keyed property load via |ic|.
  void DoKeyedLoadIC(Callable ic, compiler::InterpreterAssembler* assembler);

  // Generates code to perform a namedproperty store via |ic|.
  void DoStoreIC(Callable ic, compiler::InterpreterAssembler* assembler);

  // Generates code to perform a keyed property store via |ic|.
  void DoKeyedStoreIC(Callable ic, compiler::InterpreterAssembler* assembler);

  // Generates code to perform a JS call.
  void DoJSCall(compiler::InterpreterAssembler* assembler);

  // Generates code ro create a literal via |function_id|.
  void DoCreateLiteral(Runtime::FunctionId function_id,
                       compiler::InterpreterAssembler* assembler);

  // Generates code to perform delete via function_id.
  void DoDelete(Runtime::FunctionId function_id,
                compiler::InterpreterAssembler* assembler);

  // Generates code to perform a lookup slot load via |function_id|.
  void DoLoadLookupSlot(Runtime::FunctionId function_id,
                        compiler::InterpreterAssembler* assembler);

  // Generates code to perform a lookup slot store depending on |language_mode|.
  void DoStoreLookupSlot(LanguageMode language_mode,
                         compiler::InterpreterAssembler* assembler);

  bool IsInterpreterTableInitialized(Handle<FixedArray> handler_table);

  Isolate* isolate_;

  DISALLOW_COPY_AND_ASSIGN(Interpreter);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_INTERPRETER_H_
