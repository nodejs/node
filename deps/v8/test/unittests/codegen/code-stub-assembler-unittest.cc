// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/codegen/code-stub-assembler-unittest.h"

#include "src/codegen/code-factory.h"
#include "src/codegen/interface-descriptors.h"
#include "src/compiler/node.h"
#include "src/execution/isolate.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/node-test-utils.h"

using ::testing::_;

namespace c = v8::internal::compiler;

namespace v8 {
namespace internal {

CodeStubAssemblerTestState::CodeStubAssemblerTestState(
    CodeStubAssemblerTest* test)
    : compiler::CodeAssemblerState(
          test->isolate(), test->zone(), VoidDescriptor{},
          CodeKind::FOR_TESTING, "test",
          PoisoningMitigationLevel::kPoisonCriticalOnly) {}

TARGET_TEST_F(CodeStubAssemblerTest, SmiTag) {
  CodeStubAssemblerTestState state(this);
  CodeStubAssemblerForTest m(&state);
  TNode<IntPtrT> value = m.IntPtrConstant(44);
  EXPECT_THAT(m.SmiTag(value),
              IsBitcastWordToTaggedSigned(c::IsIntPtrConstant(
                  static_cast<intptr_t>(44) << (kSmiShiftSize + kSmiTagSize))));
  EXPECT_THAT(m.SmiUntag(m.ReinterpretCast<Smi>(value)),
              c::IsIntPtrConstant(static_cast<intptr_t>(44) >>
                                  (kSmiShiftSize + kSmiTagSize)));
}

TARGET_TEST_F(CodeStubAssemblerTest, IntPtrMax) {
  CodeStubAssemblerTestState state(this);
  CodeStubAssemblerForTest m(&state);
  {
    TNode<IntPtrT> a = m.IntPtrConstant(100);
    TNode<IntPtrT> b = m.IntPtrConstant(1);
    TNode<IntPtrT> z = m.IntPtrMax(a, b);
    EXPECT_THAT(z, c::IsIntPtrConstant(100));
  }
}

TARGET_TEST_F(CodeStubAssemblerTest, IntPtrMin) {
  CodeStubAssemblerTestState state(this);
  CodeStubAssemblerForTest m(&state);
  {
    TNode<IntPtrT> a = m.IntPtrConstant(100);
    TNode<IntPtrT> b = m.IntPtrConstant(1);
    TNode<IntPtrT> z = m.IntPtrMin(a, b);
    EXPECT_THAT(z, c::IsIntPtrConstant(1));
  }
}

}  // namespace internal
}  // namespace v8
