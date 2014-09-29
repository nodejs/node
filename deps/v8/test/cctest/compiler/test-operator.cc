// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/compiler/operator.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

#define NaN (v8::base::OS::nan_value())
#define Infinity (std::numeric_limits<double>::infinity())

TEST(TestOperatorMnemonic) {
  SimpleOperator op1(10, 0, 0, 0, "ThisOne");
  CHECK_EQ(0, strcmp(op1.mnemonic(), "ThisOne"));

  SimpleOperator op2(11, 0, 0, 0, "ThatOne");
  CHECK_EQ(0, strcmp(op2.mnemonic(), "ThatOne"));

  Operator1<int> op3(12, 0, 0, 1, "Mnemonic1", 12333);
  CHECK_EQ(0, strcmp(op3.mnemonic(), "Mnemonic1"));

  Operator1<double> op4(13, 0, 0, 1, "TheOther", 99.9);
  CHECK_EQ(0, strcmp(op4.mnemonic(), "TheOther"));
}


TEST(TestSimpleOperatorHash) {
  SimpleOperator op1(17, 0, 0, 0, "Another");
  CHECK_EQ(17, op1.HashCode());

  SimpleOperator op2(18, 0, 0, 0, "Falsch");
  CHECK_EQ(18, op2.HashCode());
}


TEST(TestSimpleOperatorEquals) {
  SimpleOperator op1a(19, 0, 0, 0, "Another1");
  SimpleOperator op1b(19, 2, 2, 2, "Another2");

  CHECK(op1a.Equals(&op1a));
  CHECK(op1a.Equals(&op1b));
  CHECK(op1b.Equals(&op1a));
  CHECK(op1b.Equals(&op1b));

  SimpleOperator op2a(20, 0, 0, 0, "Falsch1");
  SimpleOperator op2b(20, 1, 1, 1, "Falsch2");

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


static SmartArrayPointer<const char> OperatorToString(Operator* op) {
  OStringStream os;
  os << *op;
  return SmartArrayPointer<const char>(StrDup(os.c_str()));
}


TEST(TestSimpleOperatorPrint) {
  SimpleOperator op1a(19, 0, 0, 0, "Another1");
  SimpleOperator op1b(19, 2, 2, 2, "Another2");

  CHECK_EQ("Another1", OperatorToString(&op1a).get());
  CHECK_EQ("Another2", OperatorToString(&op1b).get());

  SimpleOperator op2a(20, 0, 0, 0, "Flog1");
  SimpleOperator op2b(20, 1, 1, 1, "Flog2");

  CHECK_EQ("Flog1", OperatorToString(&op2a).get());
  CHECK_EQ("Flog2", OperatorToString(&op2b).get());
}


TEST(TestOperator1intHash) {
  Operator1<int> op1a(23, 0, 0, 0, "Wolfie", 11);
  Operator1<int> op1b(23, 2, 2, 2, "Doggie", 11);

  CHECK_EQ(op1a.HashCode(), op1b.HashCode());

  Operator1<int> op2a(24, 0, 0, 0, "Arfie", 3);
  Operator1<int> op2b(24, 0, 0, 0, "Arfie", 4);

  CHECK_NE(op1a.HashCode(), op2a.HashCode());
  CHECK_NE(op2a.HashCode(), op2b.HashCode());
}


TEST(TestOperator1intEquals) {
  Operator1<int> op1a(23, 0, 0, 0, "Scratchy", 11);
  Operator1<int> op1b(23, 2, 2, 2, "Scratchy", 11);

  CHECK(op1a.Equals(&op1a));
  CHECK(op1a.Equals(&op1b));
  CHECK(op1b.Equals(&op1a));
  CHECK(op1b.Equals(&op1b));

  Operator1<int> op2a(24, 0, 0, 0, "Im", 3);
  Operator1<int> op2b(24, 0, 0, 0, "Im", 4);

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

  SimpleOperator op3(25, 0, 0, 0, "Weepy");

  CHECK(!op1a.Equals(&op3));
  CHECK(!op1b.Equals(&op3));
  CHECK(!op2a.Equals(&op3));
  CHECK(!op2b.Equals(&op3));

  CHECK(!op3.Equals(&op1a));
  CHECK(!op3.Equals(&op1b));
  CHECK(!op3.Equals(&op2a));
  CHECK(!op3.Equals(&op2b));
}


TEST(TestOperator1intPrint) {
  Operator1<int> op1(12, 0, 0, 1, "Op1Test", 0);
  CHECK_EQ("Op1Test[0]", OperatorToString(&op1).get());

  Operator1<int> op2(12, 0, 0, 1, "Op1Test", 66666666);
  CHECK_EQ("Op1Test[66666666]", OperatorToString(&op2).get());

  Operator1<int> op3(12, 0, 0, 1, "FooBar", 2347);
  CHECK_EQ("FooBar[2347]", OperatorToString(&op3).get());

  Operator1<int> op4(12, 0, 0, 1, "BarFoo", -879);
  CHECK_EQ("BarFoo[-879]", OperatorToString(&op4).get());
}


TEST(TestOperator1doubleHash) {
  Operator1<double> op1a(23, 0, 0, 0, "Wolfie", 11.77);
  Operator1<double> op1b(23, 2, 2, 2, "Doggie", 11.77);

  CHECK_EQ(op1a.HashCode(), op1b.HashCode());

  Operator1<double> op2a(24, 0, 0, 0, "Arfie", -6.7);
  Operator1<double> op2b(24, 0, 0, 0, "Arfie", -6.8);

  CHECK_NE(op1a.HashCode(), op2a.HashCode());
  CHECK_NE(op2a.HashCode(), op2b.HashCode());
}


TEST(TestOperator1doubleEquals) {
  Operator1<double> op1a(23, 0, 0, 0, "Scratchy", 11.77);
  Operator1<double> op1b(23, 2, 2, 2, "Scratchy", 11.77);

  CHECK(op1a.Equals(&op1a));
  CHECK(op1a.Equals(&op1b));
  CHECK(op1b.Equals(&op1a));
  CHECK(op1b.Equals(&op1b));

  Operator1<double> op2a(24, 0, 0, 0, "Im", 3.1);
  Operator1<double> op2b(24, 0, 0, 0, "Im", 3.2);

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

  SimpleOperator op3(25, 0, 0, 0, "Weepy");

  CHECK(!op1a.Equals(&op3));
  CHECK(!op1b.Equals(&op3));
  CHECK(!op2a.Equals(&op3));
  CHECK(!op2b.Equals(&op3));

  CHECK(!op3.Equals(&op1a));
  CHECK(!op3.Equals(&op1b));
  CHECK(!op3.Equals(&op2a));
  CHECK(!op3.Equals(&op2b));

  Operator1<double> op4a(24, 0, 0, 0, "Bashful", NaN);
  Operator1<double> op4b(24, 0, 0, 0, "Bashful", NaN);

  CHECK(op4a.Equals(&op4a));
  CHECK(op4a.Equals(&op4b));
  CHECK(op4b.Equals(&op4a));
  CHECK(op4b.Equals(&op4b));

  CHECK(!op3.Equals(&op4a));
  CHECK(!op3.Equals(&op4b));
  CHECK(!op3.Equals(&op4a));
  CHECK(!op3.Equals(&op4b));
}


TEST(TestOperator1doublePrint) {
  Operator1<double> op1(12, 0, 0, 1, "Op1Test", 0);
  CHECK_EQ("Op1Test[0]", OperatorToString(&op1).get());

  Operator1<double> op2(12, 0, 0, 1, "Op1Test", 7.3);
  CHECK_EQ("Op1Test[7.3]", OperatorToString(&op2).get());

  Operator1<double> op3(12, 0, 0, 1, "FooBar", 2e+123);
  CHECK_EQ("FooBar[2e+123]", OperatorToString(&op3).get());

  Operator1<double> op4(12, 0, 0, 1, "BarFoo", Infinity);
  CHECK_EQ("BarFoo[inf]", OperatorToString(&op4).get());

  Operator1<double> op5(12, 0, 0, 1, "BarFoo", NaN);
  CHECK_EQ("BarFoo[nan]", OperatorToString(&op5).get());
}
