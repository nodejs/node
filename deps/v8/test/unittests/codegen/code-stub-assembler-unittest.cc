// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/codegen/code-stub-assembler-unittest.h"

#include "src/compiler/node.h"
#include "src/execution/isolate.h"
#include "test/common/code-assembler-tester.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/function-tester.h"
#include "test/unittests/compiler/node-test-utils.h"

using ::testing::_;

namespace c = v8::internal::compiler;

namespace v8 {
namespace internal {

CodeStubAssemblerTestState::CodeStubAssemblerTestState(
    CodeStubAssemblerTest* test)
    : compiler::CodeAssemblerState(test->i_isolate(), test->zone(),
                                   VoidDescriptor{}, CodeKind::FOR_TESTING,
                                   "test") {}

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

#define __ assembler.

namespace {

void ExpectArrayListsEqual(DirectHandle<ArrayList> array1,
                           DirectHandle<ArrayList> array2) {
  const uint32_t array1_len = array1->ulength().value();
  EXPECT_EQ(array1->capacity(), array2->capacity());
  EXPECT_EQ(array1_len, array2->ulength().value());
  for (uint32_t i = 0; i < array1_len; i++) {
    EXPECT_EQ(array1->get(i), array2->get(i));
  }
}

}  // namespace

TARGET_TEST_F(CodeStubAssemblerTest, ArrayListAllocateEquivalent) {
  constexpr uint32_t L = 1;

  // Tests that the CSA implementation of ArrayList behave the same as the C++
  // implementation.
  DirectHandle<Code> allocate_arraylist_in_csa;
  {
    compiler::CodeAssemblerTester tester(i_isolate(), JSParameterCount(0));
    CodeStubAssembler assembler(tester.state());
    TNode<ArrayList> array = __ AllocateArrayList(__ Uint32Constant(L));
    __ ArrayListSet(array, __ Uint32Constant(0), __ UndefinedConstant());
    __ Return(array);
    allocate_arraylist_in_csa = tester.GenerateCodeCloseAndEscape();
  }

  DirectHandle<ArrayList> array1 = ArrayList::New(i_isolate(), L);
  compiler::FunctionTester ft(i_isolate(), allocate_arraylist_in_csa, 0);
  DirectHandle<ArrayList> array2 = ft.CallChecked<ArrayList>();
  ExpectArrayListsEqual(array1, array2);
}

TARGET_TEST_F(CodeStubAssemblerTest, ArrayListAddEquivalent) {
  constexpr uint32_t L = 1;

  // Tests that the CSA implementation of ArrayList behave the same as the C++
  // implementation.
  DirectHandle<Code> allocate_arraylist_in_csa;
  {
    compiler::CodeAssemblerTester tester(i_isolate(), JSParameterCount(0));
    CodeStubAssembler assembler(tester.state());
    TNode<ArrayList> array = __ AllocateArrayList(__ Uint32Constant(L));
    array = __ ArrayListAdd(array, __ SmiConstant(0));
    array = __ ArrayListAdd(array, __ SmiConstant(1));
    array = __ ArrayListAdd(array, __ SmiConstant(2));
    array = __ ArrayListAdd(array, __ SmiConstant(3));
    array = __ ArrayListAdd(array, __ SmiConstant(4));
    __ Return(array);
    allocate_arraylist_in_csa = tester.GenerateCodeCloseAndEscape();
  }

  DirectHandle<ArrayList> array1 = ArrayList::New(i_isolate(), L);
  for (int i = 0; i < 5; i++) {
    array1 = ArrayList::Add(i_isolate(), array1, Smi::FromInt(i));
  }
  compiler::FunctionTester ft(i_isolate(), allocate_arraylist_in_csa, 0);
  DirectHandle<ArrayList> list2 = ft.CallChecked<ArrayList>();
  ExpectArrayListsEqual(array1, list2);
}

TARGET_TEST_F(CodeStubAssemblerTest, ArrayListElementsEquivalent) {
  constexpr uint32_t L = 1;

  // Tests that the CSA implementation of ArrayList behave the same as the C++
  // implementation.
  DirectHandle<Code> allocate_arraylist_in_csa;
  {
    compiler::CodeAssemblerTester tester(i_isolate(), JSParameterCount(0));
    CodeStubAssembler assembler(tester.state());
    TNode<ArrayList> list = __ AllocateArrayList(__ Uint32Constant(L));
    list = __ ArrayListAdd(list, __ SmiConstant(0));
    list = __ ArrayListAdd(list, __ SmiConstant(1));
    list = __ ArrayListAdd(list, __ SmiConstant(2));
    list = __ ArrayListAdd(list, __ SmiConstant(3));
    list = __ ArrayListAdd(list, __ SmiConstant(4));
    __ Return(__ ArrayListElements(list));
    allocate_arraylist_in_csa = tester.GenerateCodeCloseAndEscape();
  }

  DirectHandle<ArrayList> array1 = ArrayList::New(i_isolate(), L);
  for (int i = 0; i < 5; i++) {
    array1 = ArrayList::Add(i_isolate(), array1, Smi::FromInt(i));
  }
  DirectHandle<FixedArray> elements1 =
      ArrayList::ToFixedArray(i_isolate(), array1);
  compiler::FunctionTester ft(i_isolate(), allocate_arraylist_in_csa, 0);
  DirectHandle<FixedArray> elements2 = ft.CallChecked<FixedArray>();
  const uint32_t elements1_len = elements1->length().value();
  const uint32_t elements2_len = elements2->length().value();
  EXPECT_EQ(elements1_len, elements2_len);
  for (uint32_t i = 0; i < elements1_len; i++) {
    EXPECT_EQ(elements1->get(i), elements2->get(i));
  }
}

TARGET_TEST_F(CodeStubAssemblerTest, ShortCircuitLogicalOps) {
  DirectHandle<Code> code;
  {
    compiler::CodeAssemblerTester tester(i_isolate(), JSParameterCount(0));
    CodeStubAssembler assembler(tester.state());
    TNode<MaybeObject> weak_undef = __ MakeWeak(__ UndefinedConstant());

    // Without short-circuiting, CAST(weak_undef) in the un-taken branches would
    // fail the CSA_DCHECK in CAST(MaybeObject).
    TNode<BoolT> and_false = __ LogicalAnd(__ IsStrong(weak_undef), [&] {
      return __ IsUndefined(__ CAST(weak_undef));
    });
    TNode<BoolT> or_true = __ LogicalOr(__ IsWeakOrCleared(weak_undef), [&] {
      return __ IsUndefined(__ CAST(weak_undef));
    });
    TNode<BoolT> any_true = __ LogicalAny(
        __ IsStrong(weak_undef),
        [&] {
          return __ LogicalAnd(__ IsStrong(weak_undef), [&] {
            return __ IsUndefined(__ CAST(weak_undef));
          });
        },
        [&] { return __ IsWeakOrCleared(weak_undef); },
        [&] { return __ IsUndefined(__ CAST(weak_undef)); });

    TNode<BoolT> ok = __ LogicalAnd(__ Word32BinaryNot(and_false),
                                    __ LogicalAnd(or_true, any_true));
    __ Return(__ SelectBooleanConstant(ok));
    code = tester.GenerateCodeCloseAndEscape();
  }

  compiler::FunctionTester ft(i_isolate(), code, 0);
  DirectHandle<Oddball> result = ft.CallChecked<Oddball>();
  EXPECT_TRUE(IsTrue(*result));
}

#undef __

}  // namespace internal
}  // namespace v8
