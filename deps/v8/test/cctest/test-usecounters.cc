// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "test/cctest/cctest.h"

namespace {

int* global_use_counts = NULL;

void MockUseCounterCallback(v8::Isolate* isolate,
                            v8::Isolate::UseCounterFeature feature) {
  ++global_use_counts[feature];
}
}

TEST(DefineGetterSetterThrowUseCount) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  int use_counts[v8::Isolate::kUseCounterFeatureCount] = {};
  global_use_counts = use_counts;
  CcTest::isolate()->SetUseCounterCallback(MockUseCounterCallback);

  // __defineGetter__ and __defineSetter__ do not increment
  // kDefineGetterOrSetterWouldThrow on success
  CompileRun(
      "var a = {};"
      "Object.defineProperty(a, 'b', { value: 0, configurable: true });"
      "a.__defineGetter__('b', ()=>{});");
  CHECK_EQ(0, use_counts[v8::Isolate::kDefineGetterOrSetterWouldThrow]);
  CompileRun(
      "var a = {};"
      "Object.defineProperty(a, 'b', { value: 0, configurable: true });"
      "a.__defineSetter__('b', ()=>{});");
  CHECK_EQ(0, use_counts[v8::Isolate::kDefineGetterOrSetterWouldThrow]);

  // __defineGetter__ and __defineSetter__ do not increment
  // kDefineGetterOrSetterWouldThrow on other errors
  v8::Local<v8::Value> resultProxyThrow = CompileRun(
      "var exception;"
      "try {"
      "var a = new Proxy({}, { defineProperty: ()=>{throw new Error;} });"
      "a.__defineGetter__('b', ()=>{});"
      "} catch (e) { exception = e; }"
      "exception");
  CHECK_EQ(0, use_counts[v8::Isolate::kDefineGetterOrSetterWouldThrow]);
  CHECK(resultProxyThrow->IsObject());
  resultProxyThrow = CompileRun(
      "var exception;"
      "try {"
      "var a = new Proxy({}, { defineProperty: ()=>{throw new Error;} });"
      "a.__defineSetter__('b', ()=>{});"
      "} catch (e) { exception = e; }"
      "exception");
  CHECK_EQ(0, use_counts[v8::Isolate::kDefineGetterOrSetterWouldThrow]);
  CHECK(resultProxyThrow->IsObject());

  // __defineGetter__ and __defineSetter__ increment
  // kDefineGetterOrSetterWouldThrow when they would throw per spec (B.2.2.2)
  CompileRun(
      "var a = {};"
      "Object.defineProperty(a, 'b', { value: 0, configurable: false });"
      "a.__defineGetter__('b', ()=>{});");
  CHECK_EQ(1, use_counts[v8::Isolate::kDefineGetterOrSetterWouldThrow]);
  CompileRun(
      "var a = {};"
      "Object.defineProperty(a, 'b', { value: 0, configurable: false });"
      "a.__defineSetter__('b', ()=>{});");
  CHECK_EQ(2, use_counts[v8::Isolate::kDefineGetterOrSetterWouldThrow]);
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
