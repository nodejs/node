// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_INTERPRETER_ASSEMBLER_UNITTEST_H_
#define V8_UNITTESTS_COMPILER_INTERPRETER_ASSEMBLER_UNITTEST_H_

#include "src/compiler/interpreter-assembler.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

namespace v8 {
namespace internal {
namespace compiler {

using ::testing::Matcher;

class InterpreterAssemblerTest : public TestWithIsolateAndZone {
 public:
  InterpreterAssemblerTest() {}
  ~InterpreterAssemblerTest() override {}

  class InterpreterAssemblerForTest final : public InterpreterAssembler {
   public:
    InterpreterAssemblerForTest(InterpreterAssemblerTest* test,
                                interpreter::Bytecode bytecode)
        : InterpreterAssembler(test->isolate(), test->zone(), bytecode) {}
    ~InterpreterAssemblerForTest() override {}

    Graph* GetCompletedGraph();

    Matcher<Node*> IsLoad(const Matcher<LoadRepresentation>& rep_matcher,
                          const Matcher<Node*>& base_matcher,
                          const Matcher<Node*>& index_matcher);
    Matcher<Node*> IsStore(const Matcher<StoreRepresentation>& rep_matcher,
                           const Matcher<Node*>& base_matcher,
                           const Matcher<Node*>& index_matcher,
                           const Matcher<Node*>& value_matcher);
    Matcher<Node*> IsBytecodeOperand(int operand);
    Matcher<Node*> IsBytecodeOperandSignExtended(int operand);

    using InterpreterAssembler::call_descriptor;
    using InterpreterAssembler::graph;

   private:
    DISALLOW_COPY_AND_ASSIGN(InterpreterAssemblerForTest);
  };
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_COMPILER_INTERPRETER_ASSEMBLER_UNITTEST_H_
