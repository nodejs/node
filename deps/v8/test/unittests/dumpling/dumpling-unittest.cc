// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_DUMPLING

#include <regex>
#include <string>

#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "src/dumpling/dumpling-manager.h"
#include "test/common/flag-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

class DumplingTest : public TestWithContext {
 public:
  bool CheckOutput(const std::string& output, const char* expected) {
    if (!std::regex_search(output, std::regex(expected))) {
      std::cout << "Output:" << std::endl << output << std::endl;
      std::cout << "Expected:" << std::endl << expected << std::endl;
      return false;
    }
    return true;
  }

  void RunInterpreterTest(const char* program, const char* expected) {
    i::FlagScope<bool> dumping_flag_scope(&i::v8_flags.interpreter_dumping,
                                          true);
    i::FlagScope<bool> allow_natives_syntax_scope(
        &i::v8_flags.allow_natives_syntax, true);

    v8::Isolate* isolate = this->isolate();
    v8::HandleScope scope(isolate);

    i::Isolate* i_isolate = reinterpret_cast<v8::internal::Isolate*>(isolate);
    i::DumplingManager* dm = i_isolate->dumpling_manager();
    dm->set_print_into_string(true);
    dm->PrepareForNextREPRLCycle();

    v8::Local<Value> result = RunJS(program);
    CHECK(!result.IsEmpty());
    const std::string& output = i_isolate->dumpling_manager()->GetOutput();

    EXPECT_TRUE(CheckOutput(output, expected));
    dm->FinishCurrentREPRLCycle();
  }
};

TEST_F(DumplingTest, InterpreterSmiParams) {
  const char* program =
      "function foo(x, y) {\n"
      "  return x + y;\n"
      "}\n"
      "%PrepareFunctionForOptimization(foo);"
      "foo(10, 2);\n";

  // Check that we see the start frame of "foo" with the parameters a0 and a1.
  const char* expected =
      "---I\\s+"
      "b:0\\s+"            // Bytecode offset 0
      "f:\\d+\\s+"         // Function id can be anything
      "x:<undefined>\\s+"  // Accumulator
      "n:2\\s+"            // Number of params
      "m:0\\s+"            // Number of registers
      "a0:10\\s+"
      "a1:2\\s+";

  RunInterpreterTest(program, expected);
}

}  // namespace v8

#endif  // V8_DUMPLING
