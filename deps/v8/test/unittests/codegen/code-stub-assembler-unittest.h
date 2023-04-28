// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_CODE_STUB_ASSEMBLER_UNITTEST_H_
#define V8_UNITTESTS_CODE_STUB_ASSEMBLER_UNITTEST_H_

#include "src/codegen/code-stub-assembler.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

class CodeStubAssemblerTest : public TestWithIsolateAndZone {
 public:
  CodeStubAssemblerTest() : TestWithIsolateAndZone(kCompressGraphZone) {}
  ~CodeStubAssemblerTest() override = default;
};

class CodeStubAssemblerTestState : public compiler::CodeAssemblerState {
 public:
  explicit CodeStubAssemblerTestState(CodeStubAssemblerTest* test);
};

class CodeStubAssemblerForTest : public CodeStubAssembler {
 public:
  explicit CodeStubAssemblerForTest(CodeStubAssemblerTestState* state)
      : CodeStubAssembler(state) {}
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_CODE_STUB_ASSEMBLER_UNITTEST_H_
