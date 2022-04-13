// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_INTERPRETER_INTRINSICS_GENERATOR_H_
#define V8_INTERPRETER_INTERPRETER_INTRINSICS_GENERATOR_H_

#include "src/interpreter/interpreter-assembler.h"

namespace v8 {
namespace internal {

namespace compiler {
class Node;
}  // namespace compiler

namespace interpreter {

extern TNode<Object> GenerateInvokeIntrinsic(
    InterpreterAssembler* assembler, TNode<Uint32T> function_id,
    TNode<Context> context, const InterpreterAssembler::RegListNodePair& args);

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_INTERPRETER_INTRINSICS_GENERATOR_H_
