// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_INTERPRETER_INTERPRETER_ASSEMBLER_UNITTEST_H_
#define V8_UNITTESTS_INTERPRETER_INTERPRETER_ASSEMBLER_UNITTEST_H_

#include "src/compiler/machine-operator.h"
#include "src/interpreter/interpreter-assembler.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

namespace v8 {
namespace internal {
namespace interpreter {

using ::testing::Matcher;

class InterpreterAssemblerTest : public TestWithIsolateAndZone {
 public:
  InterpreterAssemblerTest() {}
  ~InterpreterAssemblerTest() override {}

  class InterpreterAssemblerForTest final : public InterpreterAssembler {
   public:
    InterpreterAssemblerForTest(InterpreterAssemblerTest* test,
                                Bytecode bytecode)
        : InterpreterAssembler(test->isolate(), test->zone(), bytecode) {}
    ~InterpreterAssemblerForTest() override {}

    Matcher<compiler::Node*> IsLoad(
        const Matcher<compiler::LoadRepresentation>& rep_matcher,
        const Matcher<compiler::Node*>& base_matcher,
        const Matcher<compiler::Node*>& index_matcher);
    Matcher<compiler::Node*> IsStore(
        const Matcher<compiler::StoreRepresentation>& rep_matcher,
        const Matcher<compiler::Node*>& base_matcher,
        const Matcher<compiler::Node*>& index_matcher,
        const Matcher<compiler::Node*>& value_matcher);

    Matcher<compiler::Node*> IsBytecodeOperand(int offset);
    Matcher<compiler::Node*> IsBytecodeOperandSignExtended(int offset);
    Matcher<compiler::Node*> IsBytecodeOperandShort(int offset);
    Matcher<compiler::Node*> IsBytecodeOperandShortSignExtended(int offset);

    using InterpreterAssembler::graph;

   private:
    DISALLOW_COPY_AND_ASSIGN(InterpreterAssemblerForTest);
  };
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_INTERPRETER_INTERPRETER_ASSEMBLER_UNITTEST_H_
