// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_INTERPRETER_INTERPRETER_ASSEMBLER_UNITTEST_H_
#define V8_UNITTESTS_INTERPRETER_INTERPRETER_ASSEMBLER_UNITTEST_H_

#include "src/compiler/code-assembler.h"
#include "src/compiler/machine-operator.h"
#include "src/interpreter/interpreter-assembler.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

namespace v8 {
namespace internal {
namespace interpreter {
namespace interpreter_assembler_unittest {

using ::testing::Matcher;

class InterpreterAssemblerTest;

class InterpreterAssemblerTestState : public compiler::CodeAssemblerState {
 public:
  InterpreterAssemblerTestState(InterpreterAssemblerTest* test,
                                Bytecode bytecode);
};

class InterpreterAssemblerTest : public TestWithIsolateAndZone {
 public:
  InterpreterAssemblerTest() = default;
  ~InterpreterAssemblerTest() override = default;

  class InterpreterAssemblerForTest final : public InterpreterAssembler {
   public:
    InterpreterAssemblerForTest(
        InterpreterAssemblerTestState* state, Bytecode bytecode,
        OperandScale operand_scale = OperandScale::kSingle)
        : InterpreterAssembler(state, bytecode, operand_scale) {}
    ~InterpreterAssemblerForTest();

    Matcher<compiler::Node*> IsLoad(
        const Matcher<compiler::LoadRepresentation>& rep_matcher,
        const Matcher<compiler::Node*>& base_matcher,
        const Matcher<compiler::Node*>& index_matcher,
        LoadSensitivity needs_poisoning = LoadSensitivity::kSafe);
    Matcher<compiler::Node*> IsLoadFromObject(
        const Matcher<compiler::LoadRepresentation>& rep_matcher,
        const Matcher<compiler::Node*>& base_matcher,
        const Matcher<compiler::Node*>& index_matcher);
    Matcher<compiler::Node*> IsStore(
        const Matcher<compiler::StoreRepresentation>& rep_matcher,
        const Matcher<compiler::Node*>& base_matcher,
        const Matcher<compiler::Node*>& index_matcher,
        const Matcher<compiler::Node*>& value_matcher);

    Matcher<Node*> IsWordNot(const Matcher<Node*>& value_matcher);

    Matcher<compiler::Node*> IsUnsignedByteOperand(
        int offset, LoadSensitivity needs_poisoning);
    Matcher<compiler::Node*> IsSignedByteOperand(
        int offset, LoadSensitivity needs_poisoning);
    Matcher<compiler::Node*> IsUnsignedShortOperand(
        int offset, LoadSensitivity needs_poisoning);
    Matcher<compiler::Node*> IsSignedShortOperand(
        int offset, LoadSensitivity needs_poisoning);
    Matcher<compiler::Node*> IsUnsignedQuadOperand(
        int offset, LoadSensitivity needs_poisoning);
    Matcher<compiler::Node*> IsSignedQuadOperand(
        int offset, LoadSensitivity needs_poisoning);

    Matcher<compiler::Node*> IsUnpoisonedSignedOperand(
        int offset, OperandSize operand_size, LoadSensitivity needs_poisoning);
    Matcher<compiler::Node*> IsUnpoisonedUnsignedOperand(
        int offset, OperandSize operand_size, LoadSensitivity needs_poisoning);

    Matcher<compiler::Node*> IsSignedOperand(int offset,
                                             OperandSize operand_size,
                                             LoadSensitivity needs_poisoning);
    Matcher<compiler::Node*> IsUnsignedOperand(int offset,
                                               OperandSize operand_size,
                                               LoadSensitivity needs_poisoning);

    Matcher<compiler::Node*> IsLoadRegisterOperand(int offset,
                                                   OperandSize operand_size);

   private:
    DISALLOW_COPY_AND_ASSIGN(InterpreterAssemblerForTest);
  };
};

}  // namespace interpreter_assembler_unittest
}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_INTERPRETER_INTERPRETER_ASSEMBLER_UNITTEST_H_
