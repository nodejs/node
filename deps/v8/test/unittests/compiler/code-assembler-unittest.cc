// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/compiler/code-assembler-unittest.h"

#include "src/code-factory.h"
#include "src/compiler/node.h"
#include "src/interface-descriptors.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/node-test-utils.h"

using ::testing::_;

namespace v8 {
namespace internal {
namespace compiler {

CodeAssemblerTestState::CodeAssemblerTestState(CodeAssemblerTest* test)
    : CodeAssemblerState(test->isolate(), test->zone(),
                         VoidDescriptor(test->isolate()), Code::STUB, "test",
                         PoisoningMitigationLevel::kOn) {}

TARGET_TEST_F(CodeAssemblerTest, IntPtrAdd) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    Node* b = m.Int32Constant(1);
    Node* add = m.IntPtrAdd(a, b);
    EXPECT_THAT(add, IsIntPtrAdd(a, b));
  }
  // x + 0  =>  x
  {
    Node* a = m.Parameter(0);
    Node* b = m.Int32Constant(0);
    Node* add = m.IntPtrAdd(a, b);
    EXPECT_THAT(add, a);
  }
  // 0 + x  => x
  {
    Node* a = m.Parameter(0);
    Node* b = m.Int32Constant(0);
    Node* add = m.IntPtrAdd(b, a);
    EXPECT_THAT(add, a);
  }
  // CONST_a + CONST_b => CONST_c
  {
    Node* a = m.IntPtrConstant(22);
    Node* b = m.IntPtrConstant(33);
    Node* c = m.IntPtrAdd(a, b);
    EXPECT_THAT(c, IsIntPtrConstant(55));
  }
}

TARGET_TEST_F(CodeAssemblerTest, IntPtrSub) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    Node* b = m.Int32Constant(1);
    Node* sub = m.IntPtrSub(a, b);
    EXPECT_THAT(sub, IsIntPtrSub(a, b));
  }
  // x - 0  => x
  {
    Node* a = m.Parameter(0);
    Node* b = m.Int32Constant(0);
    Node* c = m.IntPtrSub(a, b);
    EXPECT_THAT(c, a);
  }
  // CONST_a - CONST_b  => CONST_c
  {
    Node* a = m.IntPtrConstant(100);
    Node* b = m.IntPtrConstant(1);
    Node* c = m.IntPtrSub(a, b);
    EXPECT_THAT(c, IsIntPtrConstant(99));
  }
}

TARGET_TEST_F(CodeAssemblerTest, IntPtrMul) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    Node* b = m.Int32Constant(100);
    Node* mul = m.IntPtrMul(a, b);
    EXPECT_THAT(mul, IsIntPtrMul(a, b));
  }
  // x * 1  => x
  {
    Node* a = m.Parameter(0);
    Node* b = m.Int32Constant(1);
    Node* mul = m.IntPtrMul(a, b);
    EXPECT_THAT(mul, a);
  }
  // 1 * x  => x
  {
    Node* a = m.Parameter(0);
    Node* b = m.Int32Constant(1);
    Node* mul = m.IntPtrMul(b, a);
    EXPECT_THAT(mul, a);
  }
  // CONST_a * CONST_b  => CONST_c
  {
    Node* a = m.IntPtrConstant(100);
    Node* b = m.IntPtrConstant(5);
    Node* c = m.IntPtrMul(a, b);
    EXPECT_THAT(c, IsIntPtrConstant(500));
  }
  // x * 2^CONST  => x << CONST
  {
    Node* a = m.Parameter(0);
    Node* b = m.IntPtrConstant(1 << 3);
    Node* c = m.IntPtrMul(a, b);
    EXPECT_THAT(c, IsWordShl(a, IsIntPtrConstant(3)));
  }
  // 2^CONST * x  => x << CONST
  {
    Node* a = m.IntPtrConstant(1 << 3);
    Node* b = m.Parameter(0);
    Node* c = m.IntPtrMul(a, b);
    EXPECT_THAT(c, IsWordShl(b, IsIntPtrConstant(3)));
  }
}

TARGET_TEST_F(CodeAssemblerTest, WordShl) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    Node* add = m.WordShl(a, 10);
    EXPECT_THAT(add, IsWordShl(a, IsIntPtrConstant(10)));
  }
  // x << 0  => x
  {
    Node* a = m.Parameter(0);
    Node* add = m.WordShl(a, 0);
    EXPECT_THAT(add, a);
  }
  // CONST_a << CONST_b  => CONST_c
  {
    Node* a = m.IntPtrConstant(1024);
    Node* shl = m.WordShl(a, 2);
    EXPECT_THAT(shl, IsIntPtrConstant(4096));
  }
}

TARGET_TEST_F(CodeAssemblerTest, WordShr) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    Node* shr = m.WordShr(a, 10);
    EXPECT_THAT(shr, IsWordShr(a, IsIntPtrConstant(10)));
  }
  // x >> 0  => x
  {
    Node* a = m.Parameter(0);
    Node* shr = m.WordShr(a, 0);
    EXPECT_THAT(shr, a);
  }
  // +CONST_a >> CONST_b  => CONST_c
  {
    Node* a = m.IntPtrConstant(4096);
    Node* shr = m.WordShr(a, 2);
    EXPECT_THAT(shr, IsIntPtrConstant(1024));
  }
  // -CONST_a >> CONST_b  => CONST_c
  {
    Node* a = m.IntPtrConstant(-1234);
    Node* shr = m.WordShr(a, 2);
    EXPECT_THAT(shr, IsIntPtrConstant(static_cast<uintptr_t>(-1234) >> 2));
  }
}

TARGET_TEST_F(CodeAssemblerTest, WordSar) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    Node* sar = m.WordSar(a, m.IntPtrConstant(10));
    EXPECT_THAT(sar, IsWordSar(a, IsIntPtrConstant(10)));
  }
  // x >>> 0  => x
  {
    Node* a = m.Parameter(0);
    Node* sar = m.WordSar(a, m.IntPtrConstant(0));
    EXPECT_THAT(sar, a);
  }
  // +CONST_a >>> CONST_b  => CONST_c
  {
    Node* a = m.IntPtrConstant(4096);
    Node* sar = m.WordSar(a, m.IntPtrConstant(2));
    EXPECT_THAT(sar, IsIntPtrConstant(1024));
  }
  // -CONST_a >>> CONST_b  => CONST_c
  {
    Node* a = m.IntPtrConstant(-1234);
    Node* sar = m.WordSar(a, m.IntPtrConstant(2));
    EXPECT_THAT(sar, IsIntPtrConstant(static_cast<intptr_t>(-1234) >> 2));
  }
}

TARGET_TEST_F(CodeAssemblerTest, WordOr) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    Node* z = m.WordOr(a, m.IntPtrConstant(8));
    EXPECT_THAT(z, IsWordOr(a, IsIntPtrConstant(8)));
  }
  // x | 0  => x
  {
    Node* a = m.Parameter(0);
    Node* z = m.WordOr(a, m.IntPtrConstant(0));
    EXPECT_THAT(z, a);
  }
  // 0 | x  => x
  {
    Node* a = m.Parameter(0);
    Node* z = m.WordOr(m.IntPtrConstant(0), a);
    EXPECT_THAT(z, a);
  }
  // CONST_a | CONST_b  => CONST_c
  {
    Node* a = m.IntPtrConstant(3);
    Node* b = m.WordOr(a, m.IntPtrConstant(7));
    EXPECT_THAT(b, IsIntPtrConstant(7));
  }
}

TARGET_TEST_F(CodeAssemblerTest, WordAnd) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    Node* z = m.WordAnd(a, m.IntPtrConstant(8));
    EXPECT_THAT(z, IsWordAnd(a, IsIntPtrConstant(8)));
  }
  // CONST_a & CONST_b  => CONST_c
  {
    Node* a = m.IntPtrConstant(3);
    Node* b = m.WordAnd(a, m.IntPtrConstant(7));
    EXPECT_THAT(b, IsIntPtrConstant(3));
  }
}

TARGET_TEST_F(CodeAssemblerTest, WordXor) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    Node* z = m.WordXor(a, m.IntPtrConstant(8));
    EXPECT_THAT(z, IsWordXor(a, IsIntPtrConstant(8)));
  }
  // CONST_a ^ CONST_b  => CONST_c
  {
    Node* a = m.IntPtrConstant(3);
    Node* b = m.WordXor(a, m.IntPtrConstant(7));
    EXPECT_THAT(b, IsIntPtrConstant(4));
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
