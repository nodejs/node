// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  global_use_counts = nullptr;
}

TEST(RegExpMatchIsTrueishOnNonJSRegExp) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  int use_counts[v8::Isolate::kUseCounterFeatureCount] = {};
  global_use_counts = use_counts;
  CcTest::isolate()->SetUseCounterCallback(MockUseCounterCallback);

  CompileRun("new RegExp(/./); new RegExp('');");
  CHECK_EQ(0, use_counts[v8::Isolate::kRegExpMatchIsTrueishOnNonJSRegExp]);
  CHECK_EQ(0, use_counts[v8::Isolate::kRegExpMatchIsFalseishOnJSRegExp]);

  CompileRun("let p = { [Symbol.match]: true }; new RegExp(p);");
  CHECK_EQ(1, use_counts[v8::Isolate::kRegExpMatchIsTrueishOnNonJSRegExp]);
  CHECK_EQ(0, use_counts[v8::Isolate::kRegExpMatchIsFalseishOnJSRegExp]);

  global_use_counts = nullptr;
}

TEST(RegExpMatchIsFalseishOnJSRegExp) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  int use_counts[v8::Isolate::kUseCounterFeatureCount] = {};
  global_use_counts = use_counts;
  CcTest::isolate()->SetUseCounterCallback(MockUseCounterCallback);

  CompileRun("new RegExp(/./); new RegExp('');");
  CHECK_EQ(0, use_counts[v8::Isolate::kRegExpMatchIsTrueishOnNonJSRegExp]);
  CHECK_EQ(0, use_counts[v8::Isolate::kRegExpMatchIsFalseishOnJSRegExp]);

  CompileRun("let p = /./; p[Symbol.match] = false; new RegExp(p);");
  CHECK_EQ(0, use_counts[v8::Isolate::kRegExpMatchIsTrueishOnNonJSRegExp]);
  CHECK_EQ(1, use_counts[v8::Isolate::kRegExpMatchIsFalseishOnJSRegExp]);

  global_use_counts = nullptr;
}

TEST(ObjectPrototypeHasElements) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  int use_counts[v8::Isolate::kUseCounterFeatureCount] = {};
  global_use_counts = use_counts;
  CcTest::isolate()->SetUseCounterCallback(MockUseCounterCallback);

  CompileRun("var o = {}; o[1] = 2;");
  CHECK_EQ(0, use_counts[v8::Isolate::kObjectPrototypeHasElements]);

  CompileRun("var o = {}; var p = {}; o.__proto__ = p; p[1] = 2;");
  CHECK_EQ(0, use_counts[v8::Isolate::kObjectPrototypeHasElements]);

  CompileRun("Object.prototype[0] = 2;");
  CHECK_EQ(1, use_counts[v8::Isolate::kObjectPrototypeHasElements]);

  global_use_counts = nullptr;
}

TEST(HoleyArrayReadthrough) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  int use_counts[v8::Isolate::kUseCounterFeatureCount] = {};
  global_use_counts = use_counts;
  CcTest::isolate()->SetUseCounterCallback(MockUseCounterCallback);

  // No readthrough: own property.
  CompileRun("var x = [1]; x[0];");
  CHECK_EQ(0, use_counts[v8::Isolate::kHoleyArrayReadthrough]);

  // Readthrough: hole in array, value on prototype.
  CompileRun("var x = [,1]; x.__proto__ = [2]; x[0];");
  CHECK_EQ(1, use_counts[v8::Isolate::kHoleyArrayReadthrough]);
  use_counts[v8::Isolate::kHoleyArrayReadthrough] = 0;

  // Readthrough: hole in array, no value on Object.prototype.
  CompileRun("var x = [,1]; x[0];");
  CHECK_EQ(0, use_counts[v8::Isolate::kHoleyArrayReadthrough]);
  use_counts[v8::Isolate::kHoleyArrayReadthrough] = 0;

  // Readthrough: hole in array, value on Object.prototype.
  CompileRun("var x = [,1]; Object.prototype[0] = 3; x[0];");
  CHECK_EQ(1, use_counts[v8::Isolate::kHoleyArrayReadthrough]);
  use_counts[v8::Isolate::kHoleyArrayReadthrough] = 0;

  // Readthrough: named property
  CompileRun("var x = []; Object.prototype.a = 3; x.a;");
  CHECK_EQ(0, use_counts[v8::Isolate::kHoleyArrayReadthrough]);
  use_counts[v8::Isolate::kHoleyArrayReadthrough] = 0;

  // No readthrough: not a JSArray.
  CompileRun("var x = {length: 1}; x.__proto__ = [4]; x[0];");
  CHECK_EQ(0, use_counts[v8::Isolate::kHoleyArrayReadthrough]);

  global_use_counts = nullptr;
}

TEST(ArrayPrototypeHasElements) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  int use_counts[v8::Isolate::kUseCounterFeatureCount] = {};
  global_use_counts = use_counts;
  CcTest::isolate()->SetUseCounterCallback(MockUseCounterCallback);

  CompileRun("var a = []; a[1] = 2;");
  CHECK_EQ(0, use_counts[v8::Isolate::kArrayPrototypeHasElements]);

  CompileRun("var a = []; var p = []; a.__proto__ = p; p[1] = 2;");
  CHECK_EQ(0, use_counts[v8::Isolate::kArrayPrototypeHasElements]);

  CompileRun("Array.prototype[1] = 2;");
  CHECK_EQ(1, use_counts[v8::Isolate::kArrayPrototypeHasElements]);

  global_use_counts = nullptr;
}

}  // namespace test_usecounters
}  // namespace internal
}  // namespace v8
