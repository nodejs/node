// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/factory.h"
#include "src/heap/heap.h"
#include "src/heap/heap-inl.h"
#include "src/runtime/runtime.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class RuntimeInterpreterTest : public TestWithIsolateAndZone {
 public:
  typedef Object* (*RuntimeMethod)(int, Object**, Isolate*);

  RuntimeInterpreterTest() {}
  ~RuntimeInterpreterTest() override {}

  bool TestOperatorWithObjects(RuntimeMethod method, Handle<Object> lhs,
                               Handle<Object> rhs, bool expected);
  bool TestOperator(RuntimeMethod method, int32_t lhs, int32_t rhs,
                    bool expected);
  bool TestOperator(RuntimeMethod method, double lhs, double rhs,
                    bool expected);
  bool TestOperator(RuntimeMethod method, const char* lhs, const char* rhs,
                    bool expected);
};


bool RuntimeInterpreterTest::TestOperatorWithObjects(RuntimeMethod method,
                                                     Handle<Object> lhs,
                                                     Handle<Object> rhs,
                                                     bool expected) {
  Object* args_object[] = {*rhs, *lhs};
  Handle<Object> result =
      handle(method(2, &args_object[1], isolate()), isolate());
  CHECK(result->IsTrue() || result->IsFalse());
  return result->IsTrue() == expected;
}


bool RuntimeInterpreterTest::TestOperator(RuntimeMethod method, int32_t lhs,
                                          int32_t rhs, bool expected) {
  Handle<Object> x = isolate()->factory()->NewNumberFromInt(lhs);
  Handle<Object> y = isolate()->factory()->NewNumberFromInt(rhs);
  return TestOperatorWithObjects(method, x, y, expected);
}


bool RuntimeInterpreterTest::TestOperator(RuntimeMethod method, double lhs,
                                          double rhs, bool expected) {
  Handle<Object> x = isolate()->factory()->NewNumber(lhs);
  Handle<Object> y = isolate()->factory()->NewNumber(rhs);
  CHECK_EQ(HeapNumber::cast(*x)->value(), lhs);
  CHECK_EQ(HeapNumber::cast(*y)->value(), rhs);
  return TestOperatorWithObjects(method, x, y, expected);
}


bool RuntimeInterpreterTest::TestOperator(RuntimeMethod method, const char* lhs,
                                          const char* rhs, bool expected) {
  Handle<Object> x = isolate()->factory()->NewStringFromAsciiChecked(lhs);
  Handle<Object> y = isolate()->factory()->NewStringFromAsciiChecked(rhs);
  return TestOperatorWithObjects(method, x, y, expected);
}


TEST_F(RuntimeInterpreterTest, TestOperatorsWithIntegers) {
  int32_t inputs[] = {kMinInt, Smi::kMinValue, -17,    -1, 0, 1,
                      991,     Smi::kMaxValue, kMaxInt};
  TRACED_FOREACH(int, lhs, inputs) {
    TRACED_FOREACH(int, rhs, inputs) {
#define INTEGER_OPERATOR_CHECK(r, op, x, y) \
  CHECK(TestOperator(Runtime_Interpreter##r, x, y, x op y))
      INTEGER_OPERATOR_CHECK(Equals, ==, lhs, rhs);
      INTEGER_OPERATOR_CHECK(NotEquals, !=, lhs, rhs);
      INTEGER_OPERATOR_CHECK(StrictEquals, ==, lhs, rhs);
      INTEGER_OPERATOR_CHECK(StrictNotEquals, !=, lhs, rhs);
      INTEGER_OPERATOR_CHECK(LessThan, <, lhs, rhs);
      INTEGER_OPERATOR_CHECK(GreaterThan, >, lhs, rhs);
      INTEGER_OPERATOR_CHECK(LessThanOrEqual, <=, lhs, rhs);
      INTEGER_OPERATOR_CHECK(GreaterThanOrEqual, >=, lhs, rhs);
#undef INTEGER_OPERATOR_CHECK
    }
  }
}


TEST_F(RuntimeInterpreterTest, TestOperatorsWithDoubles) {
  double inputs[] = {std::numeric_limits<double>::min(),
                     std::numeric_limits<double>::max(),
                     -0.001,
                     0.01,
                     3.14,
                     -6.02214086e23};
  TRACED_FOREACH(double, lhs, inputs) {
    TRACED_FOREACH(double, rhs, inputs) {
#define DOUBLE_OPERATOR_CHECK(r, op, x, y) \
  CHECK(TestOperator(Runtime_Interpreter##r, x, y, x op y))
      DOUBLE_OPERATOR_CHECK(Equals, ==, lhs, rhs);
      DOUBLE_OPERATOR_CHECK(NotEquals, !=, lhs, rhs);
      DOUBLE_OPERATOR_CHECK(StrictEquals, ==, lhs, rhs);
      DOUBLE_OPERATOR_CHECK(StrictNotEquals, !=, lhs, rhs);
      DOUBLE_OPERATOR_CHECK(LessThan, <, lhs, rhs);
      DOUBLE_OPERATOR_CHECK(GreaterThan, >, lhs, rhs);
      DOUBLE_OPERATOR_CHECK(LessThanOrEqual, <=, lhs, rhs);
      DOUBLE_OPERATOR_CHECK(GreaterThanOrEqual, >=, lhs, rhs);
#undef DOUBLE_OPERATOR_CHECK
    }
  }
}


TEST_F(RuntimeInterpreterTest, TestOperatorsWithString) {
  const char* inputs[] = {"abc", "a", "def", "0"};
  TRACED_FOREACH(const char*, lhs, inputs) {
    TRACED_FOREACH(const char*, rhs, inputs) {
#define STRING_OPERATOR_CHECK(r, op, x, y)         \
  CHECK(TestOperator(Runtime_Interpreter##r, x, y, \
                     std::string(x) op std::string(y)))
      STRING_OPERATOR_CHECK(Equals, ==, lhs, rhs);
      STRING_OPERATOR_CHECK(NotEquals, !=, lhs, rhs);
      STRING_OPERATOR_CHECK(StrictEquals, ==, lhs, rhs);
      STRING_OPERATOR_CHECK(StrictNotEquals, !=, lhs, rhs);
      STRING_OPERATOR_CHECK(LessThan, <, lhs, rhs);
      STRING_OPERATOR_CHECK(GreaterThan, >, lhs, rhs);
      STRING_OPERATOR_CHECK(LessThanOrEqual, <=, lhs, rhs);
      STRING_OPERATOR_CHECK(GreaterThanOrEqual, >=, lhs, rhs);
#undef STRING_OPERATOR_CHECK
    }
  }
}


TEST_F(RuntimeInterpreterTest, ToBoolean) {
  double quiet_nan = std::numeric_limits<double>::quiet_NaN();
  std::pair<Handle<Object>, bool> cases[] = {
      std::make_pair(isolate()->factory()->NewNumberFromInt(0), false),
      std::make_pair(isolate()->factory()->NewNumberFromInt(1), true),
      std::make_pair(isolate()->factory()->NewNumberFromInt(100), true),
      std::make_pair(isolate()->factory()->NewNumberFromInt(-1), true),
      std::make_pair(isolate()->factory()->NewNumber(7.7), true),
      std::make_pair(isolate()->factory()->NewNumber(0.00001), true),
      std::make_pair(isolate()->factory()->NewNumber(quiet_nan), false),
      std::make_pair(isolate()->factory()->NewHeapNumber(0.0), false),
      std::make_pair(isolate()->factory()->undefined_value(), false),
      std::make_pair(isolate()->factory()->null_value(), false),
      std::make_pair(isolate()->factory()->true_value(), true),
      std::make_pair(isolate()->factory()->false_value(), false),
      std::make_pair(isolate()->factory()->NewStringFromStaticChars(""), false),
      std::make_pair(isolate()->factory()->NewStringFromStaticChars("_"), true),
  };

  for (size_t i = 0; i < arraysize(cases); i++) {
    auto& value_expected_tuple = cases[i];
    Object* args_object[] = {*value_expected_tuple.first};
    Handle<Object> result = handle(
        Runtime_InterpreterToBoolean(1, &args_object[0], isolate()), isolate());
    CHECK(result->IsBoolean());
    CHECK_EQ(result->IsTrue(), value_expected_tuple.second);
  }
}


}  // namespace interpreter
}  // namespace internal
}  // namespace v8
