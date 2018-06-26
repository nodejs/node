// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/code-stub-assembler-unittest.h"

#include "src/code-factory.h"
#include "src/compiler/node.h"
#include "src/interface-descriptors.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/node-test-utils.h"

using ::testing::_;
using v8::internal::compiler::Node;

namespace c = v8::internal::compiler;

namespace v8 {
namespace internal {

#ifdef ENABLE_VERIFY_CSA
#define IS_BITCAST_WORD_TO_TAGGED_SIGNED(x) IsBitcastWordToTaggedSigned(x)
#define IS_BITCAST_TAGGED_TO_WORD(x) IsBitcastTaggedToWord(x)
#else
#define IS_BITCAST_WORD_TO_TAGGED_SIGNED(x) (x)
#define IS_BITCAST_TAGGED_TO_WORD(x) (x)
#endif

CodeStubAssemblerTestState::CodeStubAssemblerTestState(
    CodeStubAssemblerTest* test)
    : compiler::CodeAssemblerState(test->isolate(), test->zone(),
                                   VoidDescriptor(test->isolate()), Code::STUB,
                                   "test", PoisoningMitigationLevel::kOn) {}

TARGET_TEST_F(CodeStubAssemblerTest, SmiTag) {
  CodeStubAssemblerTestState state(this);
  CodeStubAssemblerForTest m(&state);
  Node* value = m.Int32Constant(44);
  EXPECT_THAT(m.SmiTag(value),
              IS_BITCAST_WORD_TO_TAGGED_SIGNED(c::IsIntPtrConstant(
                  static_cast<intptr_t>(44) << (kSmiShiftSize + kSmiTagSize))));
  EXPECT_THAT(m.SmiUntag(value),
              c::IsIntPtrConstant(static_cast<intptr_t>(44) >>
                                  (kSmiShiftSize + kSmiTagSize)));
}

TARGET_TEST_F(CodeStubAssemblerTest, IntPtrMax) {
  CodeStubAssemblerTestState state(this);
  CodeStubAssemblerForTest m(&state);
  {
    Node* a = m.IntPtrConstant(100);
    Node* b = m.IntPtrConstant(1);
    Node* z = m.IntPtrMax(a, b);
    EXPECT_THAT(z, c::IsIntPtrConstant(100));
  }
}

TARGET_TEST_F(CodeStubAssemblerTest, IntPtrMin) {
  CodeStubAssemblerTestState state(this);
  CodeStubAssemblerForTest m(&state);
  {
    Node* a = m.IntPtrConstant(100);
    Node* b = m.IntPtrConstant(1);
    Node* z = m.IntPtrMin(a, b);
    EXPECT_THAT(z, c::IsIntPtrConstant(1));
  }
}

}  // namespace internal
}  // namespace v8
