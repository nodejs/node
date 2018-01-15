// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>

#include "src/compiler/operator.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

#define NONE Operator::kNoProperties
#define FOLD Operator::kFoldable


TEST(TestOperator_Mnemonic) {
  Operator op1(10, NONE, "ThisOne", 0, 0, 0, 0, 0, 0);
  CHECK_EQ(0, strcmp(op1.mnemonic(), "ThisOne"));

  Operator op2(11, NONE, "ThatOne", 0, 0, 0, 0, 0, 0);
  CHECK_EQ(0, strcmp(op2.mnemonic(), "ThatOne"));

  Operator1<int> op3(12, NONE, "Mnemonic1", 0, 0, 0, 1, 0, 0, 12333);
  CHECK_EQ(0, strcmp(op3.mnemonic(), "Mnemonic1"));

  Operator1<double> op4(13, NONE, "TheOther", 0, 0, 0, 1, 0, 0, 99.9);
  CHECK_EQ(0, strcmp(op4.mnemonic(), "TheOther"));
}


TEST(TestOperator_Hash) {
  Operator op1(17, NONE, "Another", 0, 0, 0, 0, 0, 0);
  CHECK_EQ(17, static_cast<int>(op1.HashCode()));

  Operator op2(18, NONE, "Falsch", 0, 0, 0, 0, 0, 0);
  CHECK_EQ(18, static_cast<int>(op2.HashCode()));
}


TEST(TestOperator_Equals) {
  Operator op1a(19, NONE, "Another1", 0, 0, 0, 0, 0, 0);
  Operator op1b(19, FOLD, "Another2", 2, 0, 0, 2, 0, 0);

  CHECK(op1a.Equals(&op1a));
  CHECK(op1a.Equals(&op1b));
  CHECK(op1b.Equals(&op1a));
  CHECK(op1b.Equals(&op1b));

  Operator op2a(20, NONE, "Falsch1", 0, 0, 0, 0, 0, 0);
  Operator op2b(20, FOLD, "Falsch2", 1, 0, 0, 1, 0, 0);

  CHECK(op2a.Equals(&op2a));
  CHECK(op2a.Equals(&op2b));
  CHECK(op2b.Equals(&op2a));
  CHECK(op2b.Equals(&op2b));

  CHECK(!op1a.Equals(&op2a));
  CHECK(!op1a.Equals(&op2b));
  CHECK(!op1b.Equals(&op2a));
  CHECK(!op1b.Equals(&op2b));

  CHECK(!op2a.Equals(&op1a));
  CHECK(!op2a.Equals(&op1b));
  CHECK(!op2b.Equals(&op1a));
  CHECK(!op2b.Equals(&op1b));
}

static std::unique_ptr<char[]> OperatorToString(Operator* op) {
  std::ostringstream os;
  os << *op;
  return std::unique_ptr<char[]>(StrDup(os.str().c_str()));
}


TEST(TestOperator_Print) {
  Operator op1a(19, NONE, "Another1", 0, 0, 0, 0, 0, 0);
  Operator op1b(19, FOLD, "Another2", 2, 0, 0, 2, 0, 0);

  CHECK_EQ(0, strcmp("Another1", OperatorToString(&op1a).get()));
  CHECK_EQ(0, strcmp("Another2", OperatorToString(&op1b).get()));

  Operator op2a(20, NONE, "Flog1", 0, 0, 0, 0, 0, 0);
  Operator op2b(20, FOLD, "Flog2", 1, 0, 0, 1, 0, 0);

  CHECK_EQ(0, strcmp("Flog1", OperatorToString(&op2a).get()));
  CHECK_EQ(0, strcmp("Flog2", OperatorToString(&op2b).get()));
}


TEST(TestOperator1int_Hash) {
  Operator1<int> op1a(23, NONE, "Wolfie", 0, 0, 0, 0, 0, 0, 11);
  Operator1<int> op1b(23, FOLD, "Doggie", 2, 0, 0, 2, 0, 0, 11);

  CHECK(op1a.HashCode() == op1b.HashCode());

  Operator1<int> op2a(24, NONE, "Arfie", 0, 0, 0, 0, 0, 0, 3);
  Operator1<int> op2b(24, NONE, "Arfie", 0, 0, 0, 0, 0, 0, 4);

  CHECK(op1a.HashCode() != op2a.HashCode());
  CHECK(op2a.HashCode() != op2b.HashCode());
}


TEST(TestOperator1int_Equals) {
  Operator1<int> op1a(23, NONE, "Scratchy", 0, 0, 0, 0, 0, 0, 11);
  Operator1<int> op1b(23, FOLD, "Scratchy", 2, 0, 0, 2, 0, 0, 11);

  CHECK(op1a.Equals(&op1a));
  CHECK(op1a.Equals(&op1b));
  CHECK(op1b.Equals(&op1a));
  CHECK(op1b.Equals(&op1b));

  Operator1<int> op2a(24, NONE, "Im", 0, 0, 0, 0, 0, 0, 3);
  Operator1<int> op2b(24, NONE, "Im", 0, 0, 0, 0, 0, 0, 4);

  CHECK(op2a.Equals(&op2a));
  CHECK(!op2a.Equals(&op2b));
  CHECK(!op2b.Equals(&op2a));
  CHECK(op2b.Equals(&op2b));

  CHECK(!op1a.Equals(&op2a));
  CHECK(!op1a.Equals(&op2b));
  CHECK(!op1b.Equals(&op2a));
  CHECK(!op1b.Equals(&op2b));

  CHECK(!op2a.Equals(&op1a));
  CHECK(!op2a.Equals(&op1b));
  CHECK(!op2b.Equals(&op1a));
  CHECK(!op2b.Equals(&op1b));

  Operator op3(25, NONE, "Weepy", 0, 0, 0, 0, 0, 0);

  CHECK(!op1a.Equals(&op3));
  CHECK(!op1b.Equals(&op3));
  CHECK(!op2a.Equals(&op3));
  CHECK(!op2b.Equals(&op3));

  CHECK(!op3.Equals(&op1a));
  CHECK(!op3.Equals(&op1b));
  CHECK(!op3.Equals(&op2a));
  CHECK(!op3.Equals(&op2b));
}


TEST(TestOperator1int_Print) {
  Operator1<int> op1(12, NONE, "Op1Test", 0, 0, 0, 1, 0, 0, 0);
  CHECK_EQ(0, strcmp("Op1Test[0]", OperatorToString(&op1).get()));

  Operator1<int> op2(12, NONE, "Op1Test", 0, 0, 0, 1, 0, 0, 66666666);
  CHECK_EQ(0, strcmp("Op1Test[66666666]", OperatorToString(&op2).get()));

  Operator1<int> op3(12, NONE, "FooBar", 0, 0, 0, 1, 0, 0, 2347);
  CHECK_EQ(0, strcmp("FooBar[2347]", OperatorToString(&op3).get()));

  Operator1<int> op4(12, NONE, "BarFoo", 0, 0, 0, 1, 0, 0, -879);
  CHECK_EQ(0, strcmp("BarFoo[-879]", OperatorToString(&op4).get()));
}


TEST(TestOperator1double_Hash) {
  Operator1<double> op1a(23, NONE, "Wolfie", 0, 0, 0, 0, 0, 0, 11.77);
  Operator1<double> op1b(23, FOLD, "Doggie", 2, 0, 0, 2, 0, 0, 11.77);

  CHECK(op1a.HashCode() == op1b.HashCode());

  Operator1<double> op2a(24, NONE, "Arfie", 0, 0, 0, 0, 0, 0, -6.7);
  Operator1<double> op2b(24, NONE, "Arfie", 0, 0, 0, 0, 0, 0, -6.8);

  CHECK(op1a.HashCode() != op2a.HashCode());
  CHECK(op2a.HashCode() != op2b.HashCode());
}


TEST(TestOperator1doublePrint) {
  Operator1<double> op1a(23, NONE, "Canary", 0, 0, 0, 0, 0, 0, 0.5);
  Operator1<double> op1b(23, FOLD, "Finch", 2, 0, 0, 2, 0, 0, -1.5);

  CHECK_EQ(0, strcmp("Canary[0.5]", OperatorToString(&op1a).get()));
  CHECK_EQ(0, strcmp("Finch[-1.5]", OperatorToString(&op1b).get()));
}


TEST(TestOperator1double_Equals) {
  Operator1<double> op1a(23, NONE, "Scratchy", 0, 0, 0, 0, 0, 0, 11.77);
  Operator1<double> op1b(23, FOLD, "Scratchy", 2, 0, 0, 2, 0, 0, 11.77);

  CHECK(op1a.Equals(&op1a));
  CHECK(op1a.Equals(&op1b));
  CHECK(op1b.Equals(&op1a));
  CHECK(op1b.Equals(&op1b));

  Operator1<double> op2a(24, NONE, "Im", 0, 0, 0, 0, 0, 0, 3.1);
  Operator1<double> op2b(24, NONE, "Im", 0, 0, 0, 0, 0, 0, 3.2);

  CHECK(op2a.Equals(&op2a));
  CHECK(!op2a.Equals(&op2b));
  CHECK(!op2b.Equals(&op2a));
  CHECK(op2b.Equals(&op2b));

  CHECK(!op1a.Equals(&op2a));
  CHECK(!op1a.Equals(&op2b));
  CHECK(!op1b.Equals(&op2a));
  CHECK(!op1b.Equals(&op2b));

  CHECK(!op2a.Equals(&op1a));
  CHECK(!op2a.Equals(&op1b));
  CHECK(!op2b.Equals(&op1a));
  CHECK(!op2b.Equals(&op1b));

  Operator op3(25, NONE, "Weepy", 0, 0, 0, 0, 0, 0);

  CHECK(!op1a.Equals(&op3));
  CHECK(!op1b.Equals(&op3));
  CHECK(!op2a.Equals(&op3));
  CHECK(!op2b.Equals(&op3));

  CHECK(!op3.Equals(&op1a));
  CHECK(!op3.Equals(&op1b));
  CHECK(!op3.Equals(&op2a));
  CHECK(!op3.Equals(&op2b));

  Operator1<double> op4a(24, NONE, "Bashful", 0, 0, 0, 0, 0, 0, 1.0);
  Operator1<double> op4b(24, NONE, "Bashful", 0, 0, 0, 0, 0, 0, 1.0);

  CHECK(op4a.Equals(&op4a));
  CHECK(op4a.Equals(&op4b));
  CHECK(op4b.Equals(&op4a));
  CHECK(op4b.Equals(&op4b));

  CHECK(!op3.Equals(&op4a));
  CHECK(!op3.Equals(&op4b));
  CHECK(!op3.Equals(&op4a));
  CHECK(!op3.Equals(&op4b));
}


TEST(TestOpParameter_Operator1double) {
  double values[] = {7777.5, -66, 0, 11, 0.1};

  for (size_t i = 0; i < arraysize(values); i++) {
    Operator1<double> op(33, NONE, "Scurvy", 0, 0, 0, 0, 0, 0, values[i]);
    CHECK_EQ(values[i], OpParameter<double>(&op));
  }
}


TEST(TestOpParameter_Operator1float) {
  float values[] = {// thanks C++.
                    static_cast<float>(7777.5), static_cast<float>(-66),
                    static_cast<float>(0), static_cast<float>(11),
                    static_cast<float>(0.1)};

  for (size_t i = 0; i < arraysize(values); i++) {
    Operator1<float> op(33, NONE, "Scurvy", 0, 0, 0, 0, 0, 0, values[i]);
    CHECK_EQ(values[i], OpParameter<float>(&op));
  }
}


TEST(TestOpParameter_Operator1int) {
  int values[] = {7777, -66, 0, 11, 1, 0x666aff};

  for (size_t i = 0; i < arraysize(values); i++) {
    Operator1<int> op(33, NONE, "Scurvy", 0, 0, 0, 0, 0, 0, values[i]);
    CHECK_EQ(values[i], OpParameter<int>(&op));
  }
}


TEST(Operator_CountsOrder) {
  Operator op(29, NONE, "Flashy", 11, 22, 33, 44, 55, 66);
  CHECK_EQ(11, op.ValueInputCount());
  CHECK_EQ(22, op.EffectInputCount());
  CHECK_EQ(33, op.ControlInputCount());

  CHECK_EQ(44, op.ValueOutputCount());
  CHECK_EQ(55, op.EffectOutputCount());
  CHECK_EQ(66, op.ControlOutputCount());
}

#undef NONE
#undef FOLD

}  // namespace compiler
}  // namespace internal
}  // namespace v8
