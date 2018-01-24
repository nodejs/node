// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_SETUP_INTERPRETER_H_
#define V8_INTERPRETER_SETUP_INTERPRETER_H_

#include "src/interpreter/bytecode-operands.h"
#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {
namespace interpreter {

class Interpreter;

class SetupInterpreter {
 public:
  static void InstallBytecodeHandlers(Interpreter* interpreter);

 private:
  // Generates handler for given |bytecode| and |operand_scale|
  // and installs it into the |dispatch_table|.
  static void InstallBytecodeHandler(Isolate* isolate, Address* dispatch_table,
                                     Bytecode bytecode,
                                     OperandScale operand_scale);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_SETUP_INTERPRETER_H_
