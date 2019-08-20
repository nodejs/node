// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_CODE_ASSEMBLER_UNITTEST_H_
#define V8_UNITTESTS_COMPILER_CODE_ASSEMBLER_UNITTEST_H_

#include "src/compiler/code-assembler.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

namespace v8 {
namespace internal {
namespace compiler {

class CodeAssemblerTest : public TestWithIsolateAndZone {
 public:
  CodeAssemblerTest() = default;
  ~CodeAssemblerTest() override = default;
};

class CodeAssemblerTestState : public CodeAssemblerState {
 public:
  explicit CodeAssemblerTestState(CodeAssemblerTest* test);
};

class CodeAssemblerForTest : public CodeAssembler {
 public:
  explicit CodeAssemblerForTest(CodeAssemblerTestState* state)
      : CodeAssembler(state) {}
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_COMPILER_CODE_ASSEMBLER_UNITTEST_H_
