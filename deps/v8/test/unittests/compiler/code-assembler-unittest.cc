// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/compiler/code-assembler-unittest.h"

#include "src/codegen/code-factory.h"
#include "src/codegen/interface-descriptors.h"
#include "src/compiler/node.h"
#include "src/execution/isolate.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/node-test-utils.h"

using ::testing::_;
using ::testing::Eq;

namespace v8 {
namespace internal {
namespace compiler {

CodeAssemblerTestState::CodeAssemblerTestState(CodeAssemblerTest* test)
    : CodeAssemblerState(test->isolate(), test->zone(), VoidDescriptor{},
                         Code::STUB, "test",
                         PoisoningMitigationLevel::kPoisonCriticalOnly) {}

TARGET_TEST_F(CodeAssemblerTest, IntPtrAdd) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    TNode<IntPtrT> b = m.IntPtrConstant(1);
    TNode<WordT> add = m.IntPtrAdd(a, b);
    EXPECT_THAT(add, IsIntPtrAdd(Eq(a), Eq(b)));
  }
  // x + 0  =>  x
  {
    Node* a = m.Parameter(0);
    TNode<IntPtrT> b = m.IntPtrConstant(0);
    TNode<WordT> add = m.IntPtrAdd(a, b);
    EXPECT_THAT(add, a);
  }
  // 0 + x  => x
  {
    Node* a = m.Parameter(0);
    TNode<IntPtrT> b = m.IntPtrConstant(0);
    TNode<WordT> add = m.IntPtrAdd(b, a);
    EXPECT_THAT(add, a);
  }
  // CONST_a + CONST_b => CONST_c
  {
    TNode<IntPtrT> a = m.IntPtrConstant(22);
    TNode<IntPtrT> b = m.IntPtrConstant(33);
    TNode<IntPtrT> c = m.IntPtrAdd(a, b);
    EXPECT_THAT(c, IsIntPtrConstant(55));
  }
}

TARGET_TEST_F(CodeAssemblerTest, IntPtrSub) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    TNode<IntPtrT> b = m.IntPtrConstant(1);
    TNode<WordT> sub = m.IntPtrSub(a, b);
    EXPECT_THAT(sub, IsIntPtrSub(Eq(a), Eq(b)));
  }
  // x - 0  => x
  {
    Node* a = m.Parameter(0);
    TNode<IntPtrT> b = m.IntPtrConstant(0);
    TNode<WordT> c = m.IntPtrSub(a, b);
    EXPECT_THAT(c, a);
  }
  // CONST_a - CONST_b  => CONST_c
  {
    TNode<IntPtrT> a = m.IntPtrConstant(100);
    TNode<IntPtrT> b = m.IntPtrConstant(1);
    TNode<IntPtrT> c = m.IntPtrSub(a, b);
    EXPECT_THAT(c, IsIntPtrConstant(99));
  }
}

TARGET_TEST_F(CodeAssemblerTest, IntPtrMul) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    TNode<IntPtrT> b = m.IntPtrConstant(100);
    TNode<WordT> mul = m.IntPtrMul(a, b);
    EXPECT_THAT(mul, IsIntPtrMul(Eq(a), Eq(b)));
  }
  // x * 1  => x
  {
    Node* a = m.Parameter(0);
    TNode<IntPtrT> b = m.IntPtrConstant(1);
    TNode<WordT> mul = m.IntPtrMul(a, b);
    EXPECT_THAT(mul, a);
  }
  // 1 * x  => x
  {
    Node* a = m.Parameter(0);
    TNode<IntPtrT> b = m.IntPtrConstant(1);
    TNode<WordT> mul = m.IntPtrMul(b, a);
    EXPECT_THAT(mul, a);
  }
  // CONST_a * CONST_b  => CONST_c
  {
    TNode<IntPtrT> a = m.IntPtrConstant(100);
    TNode<IntPtrT> b = m.IntPtrConstant(5);
    TNode<IntPtrT> c = m.IntPtrMul(a, b);
    EXPECT_THAT(c, IsIntPtrConstant(500));
  }
  // x * 2^CONST  => x << CONST
  {
    Node* a = m.Parameter(0);
    TNode<IntPtrT> b = m.IntPtrConstant(1 << 3);
    TNode<WordT> c = m.IntPtrMul(a, b);
    EXPECT_THAT(c, IsWordShl(a, IsIntPtrConstant(3)));
  }
  // 2^CONST * x  => x << CONST
  {
    TNode<IntPtrT> a = m.IntPtrConstant(1 << 3);
    Node* b = m.Parameter(0);
    TNode<WordT> c = m.IntPtrMul(a, b);
    EXPECT_THAT(c, IsWordShl(b, IsIntPtrConstant(3)));
  }
}

TARGET_TEST_F(CodeAssemblerTest, IntPtrDiv) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    TNode<IntPtrT> a = m.UncheckedCast<IntPtrT>(m.Parameter(0));
    TNode<IntPtrT> b = m.IntPtrConstant(100);
    TNode<IntPtrT> div = m.IntPtrDiv(a, b);
    EXPECT_THAT(div, IsIntPtrDiv(Matcher<Node*>(a), Matcher<Node*>(b)));
  }
  // x / 1  => x
  {
    TNode<IntPtrT> a = m.UncheckedCast<IntPtrT>(m.Parameter(0));
    TNode<IntPtrT> b = m.IntPtrConstant(1);
    TNode<IntPtrT> div = m.IntPtrDiv(a, b);
    EXPECT_THAT(div, a);
  }
  // CONST_a / CONST_b  => CONST_c
  {
    TNode<IntPtrT> a = m.IntPtrConstant(100);
    TNode<IntPtrT> b = m.IntPtrConstant(5);
    TNode<IntPtrT> div = m.IntPtrDiv(a, b);
    EXPECT_THAT(div, IsIntPtrConstant(20));
  }
  {
    TNode<IntPtrT> a = m.IntPtrConstant(100);
    TNode<IntPtrT> b = m.IntPtrConstant(5);
    TNode<IntPtrT> div = m.IntPtrDiv(a, b);
    EXPECT_THAT(div, IsIntPtrConstant(20));
  }
  // x / 2^CONST  => x >> CONST
  {
    TNode<IntPtrT> a = m.UncheckedCast<IntPtrT>(m.Parameter(0));
    TNode<IntPtrT> b = m.IntPtrConstant(1 << 3);
    TNode<IntPtrT> div = m.IntPtrDiv(a, b);
    EXPECT_THAT(div, IsWordSar(Matcher<Node*>(a), IsIntPtrConstant(3)));
  }
}

TARGET_TEST_F(CodeAssemblerTest, WordShl) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    TNode<WordT> add = m.WordShl(a, 10);
    EXPECT_THAT(add, IsWordShl(a, IsIntPtrConstant(10)));
  }
  // x << 0  => x
  {
    Node* a = m.Parameter(0);
    TNode<WordT> add = m.WordShl(a, 0);
    EXPECT_THAT(add, a);
  }
  // CONST_a << CONST_b  => CONST_c
  {
    TNode<IntPtrT> a = m.IntPtrConstant(1024);
    TNode<WordT> shl = m.WordShl(a, 2);
    EXPECT_THAT(shl, IsIntPtrConstant(4096));
  }
}

TARGET_TEST_F(CodeAssemblerTest, WordShr) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    TNode<WordT> shr = m.WordShr(a, 10);
    EXPECT_THAT(shr, IsWordShr(a, IsIntPtrConstant(10)));
  }
  // x >> 0  => x
  {
    Node* a = m.Parameter(0);
    TNode<WordT> shr = m.WordShr(a, 0);
    EXPECT_THAT(shr, a);
  }
  // +CONST_a >> CONST_b  => CONST_c
  {
    TNode<IntPtrT> a = m.IntPtrConstant(4096);
    TNode<IntPtrT> shr = m.WordShr(a, 2);
    EXPECT_THAT(shr, IsIntPtrConstant(1024));
  }
  // -CONST_a >> CONST_b  => CONST_c
  {
    TNode<IntPtrT> a = m.IntPtrConstant(-1234);
    TNode<IntPtrT> shr = m.WordShr(a, 2);
    EXPECT_THAT(shr, IsIntPtrConstant(static_cast<uintptr_t>(-1234) >> 2));
  }
}

TARGET_TEST_F(CodeAssemblerTest, WordSar) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    TNode<WordT> sar = m.WordSar(a, m.IntPtrConstant(10));
    EXPECT_THAT(sar, IsWordSar(a, IsIntPtrConstant(10)));
  }
  // x >>> 0  => x
  {
    Node* a = m.Parameter(0);
    TNode<WordT> sar = m.WordSar(a, m.IntPtrConstant(0));
    EXPECT_THAT(sar, a);
  }
  // +CONST_a >>> CONST_b  => CONST_c
  {
    TNode<IntPtrT> a = m.IntPtrConstant(4096);
    TNode<IntPtrT> sar = m.WordSar(a, m.IntPtrConstant(2));
    EXPECT_THAT(sar, IsIntPtrConstant(1024));
  }
  // -CONST_a >>> CONST_b  => CONST_c
  {
    TNode<IntPtrT> a = m.IntPtrConstant(-1234);
    TNode<IntPtrT> sar = m.WordSar(a, m.IntPtrConstant(2));
    EXPECT_THAT(sar, IsIntPtrConstant(static_cast<intptr_t>(-1234) >> 2));
  }
}

TARGET_TEST_F(CodeAssemblerTest, WordOr) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    TNode<WordT> z = m.WordOr(a, m.IntPtrConstant(8));
    EXPECT_THAT(z, IsWordOr(a, IsIntPtrConstant(8)));
  }
  // x | 0  => x
  {
    Node* a = m.Parameter(0);
    TNode<WordT> z = m.WordOr(a, m.IntPtrConstant(0));
    EXPECT_THAT(z, a);
  }
  // 0 | x  => x
  {
    Node* a = m.Parameter(0);
    TNode<WordT> z = m.WordOr(m.IntPtrConstant(0), a);
    EXPECT_THAT(z, a);
  }
  // CONST_a | CONST_b  => CONST_c
  {
    TNode<IntPtrT> a = m.IntPtrConstant(3);
    TNode<WordT> b = m.WordOr(a, m.IntPtrConstant(7));
    EXPECT_THAT(b, IsIntPtrConstant(7));
  }
}

TARGET_TEST_F(CodeAssemblerTest, WordAnd) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    TNode<WordT> z = m.WordAnd(a, m.IntPtrConstant(8));
    EXPECT_THAT(z, IsWordAnd(a, IsIntPtrConstant(8)));
  }
  // CONST_a & CONST_b  => CONST_c
  {
    TNode<IntPtrT> a = m.IntPtrConstant(3);
    TNode<IntPtrT> b = m.WordAnd(a, m.IntPtrConstant(7));
    EXPECT_THAT(b, IsIntPtrConstant(3));
  }
}

TARGET_TEST_F(CodeAssemblerTest, WordXor) {
  CodeAssemblerTestState state(this);
  CodeAssemblerForTest m(&state);
  {
    Node* a = m.Parameter(0);
    TNode<WordT> z = m.WordXor(a, m.IntPtrConstant(8));
    EXPECT_THAT(z, IsWordXor(a, IsIntPtrConstant(8)));
  }
  // CONST_a ^ CONST_b  => CONST_c
  {
    TNode<IntPtrT> a = m.IntPtrConstant(3);
    TNode<WordT> b = m.WordXor(a, m.IntPtrConstant(7));
    EXPECT_THAT(b, IsIntPtrConstant(4));
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
