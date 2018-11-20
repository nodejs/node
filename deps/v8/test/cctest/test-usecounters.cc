// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace test_usecounters {

int* global_use_counts = nullptr;

void MockUseCounterCallback(v8::Isolate* isolate,
                            v8::Isolate::UseCounterFeature feature) {
  ++global_use_counts[feature];
}

TEST(AssigmentExpressionLHSIsCall) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  int use_counts[v8::Isolate::kUseCounterFeatureCount] = {};
  global_use_counts = use_counts;
  CcTest::isolate()->SetUseCounterCallback(MockUseCounterCallback);

  // AssignmentExpressions whose LHS is not a call do not increment counters
  CompileRun("function f(){ a = 0; a()[b] = 0; }");
  CHECK_EQ(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy]);
  CHECK_EQ(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInStrict]);
  CompileRun("function f(){ ++a; ++a()[b]; }");
  CHECK_EQ(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy]);
  CHECK_EQ(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInStrict]);
  CompileRun("function f(){ 'use strict'; a = 0; a()[b] = 0; }");
  CHECK_EQ(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy]);
  CHECK_EQ(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInStrict]);
  CompileRun("function f(){ 'use strict'; ++a; ++a()[b]; }");
  CHECK_EQ(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy]);
  CHECK_EQ(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInStrict]);

  // AssignmentExpressions whose LHS is a call increment appropriate counters
  CompileRun("function f(){ a() = 0; }");
  CHECK_NE(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy]);
  CHECK_EQ(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInStrict]);
  use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy] = 0;
  CompileRun("function f(){ 'use strict'; a() = 0; }");
  CHECK_EQ(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy]);
  CHECK_NE(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInStrict]);
  use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInStrict] = 0;

  // UpdateExpressions whose LHS is a call increment appropriate counters
  CompileRun("function f(){ ++a(); }");
  CHECK_NE(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy]);
  CHECK_EQ(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInStrict]);
  use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy] = 0;
  CompileRun("function f(){ 'use strict'; ++a(); }");
  CHECK_EQ(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy]);
  CHECK_NE(0, use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInStrict]);
  use_counts[v8::Isolate::kAssigmentExpressionLHSIsCallInStrict] = 0;
}

}  // namespace test_usecounters
}  // namespace internal
}  // namespace v8
