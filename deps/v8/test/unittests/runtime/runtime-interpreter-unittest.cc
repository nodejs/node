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
