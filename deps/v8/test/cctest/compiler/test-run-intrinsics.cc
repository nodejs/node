// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compilation-info.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

uint32_t flags = CompilationInfo::kInliningEnabled;


TEST(Call) {
  FunctionTester T("(function(a,b) { return %_Call(b, a, 1, 2, 3); })", flags);
  CompileRun("function f(a,b,c) { return a + b + c + this.d; }");

  T.CheckCall(T.Val(129), T.NewObject("({d:123})"), T.NewObject("f"));
  T.CheckCall(T.Val("6x"), T.NewObject("({d:'x'})"), T.NewObject("f"));
}


TEST(ClassOf) {
  FunctionTester T("(function(a) { return %_ClassOf(a); })", flags);

  T.CheckCall(T.Val("Function"), T.NewObject("(function() {})"));
  T.CheckCall(T.Val("Array"), T.NewObject("([1])"));
  T.CheckCall(T.Val("Object"), T.NewObject("({})"));
  T.CheckCall(T.Val("RegExp"), T.NewObject("(/x/)"));
  T.CheckCall(T.null(), T.undefined());
  T.CheckCall(T.null(), T.null());
  T.CheckCall(T.null(), T.Val("x"));
  T.CheckCall(T.null(), T.Val(1));
}


TEST(IsArray) {
  FunctionTester T("(function(a) { return %_IsArray(a); })", flags);

  T.CheckFalse(T.NewObject("new Date()"));
  T.CheckFalse(T.NewObject("(function() {})"));
  T.CheckTrue(T.NewObject("([1])"));
  T.CheckFalse(T.NewObject("({})"));
  T.CheckFalse(T.NewObject("(/x/)"));
  T.CheckFalse(T.undefined());
  T.CheckFalse(T.null());
  T.CheckFalse(T.Val("x"));
  T.CheckFalse(T.Val(1));
}


TEST(IsDate) {
  FunctionTester T("(function(a) { return %_IsDate(a); })", flags);

  T.CheckTrue(T.NewObject("new Date()"));
  T.CheckFalse(T.NewObject("(function() {})"));
  T.CheckFalse(T.NewObject("([1])"));
  T.CheckFalse(T.NewObject("({})"));
  T.CheckFalse(T.NewObject("(/x/)"));
  T.CheckFalse(T.undefined());
  T.CheckFalse(T.null());
  T.CheckFalse(T.Val("x"));
  T.CheckFalse(T.Val(1));
}


TEST(IsFunction) {
  FunctionTester T("(function(a) { return %_IsFunction(a); })", flags);

  T.CheckFalse(T.NewObject("new Date()"));
  T.CheckTrue(T.NewObject("(function() {})"));
  T.CheckFalse(T.NewObject("([1])"));
  T.CheckFalse(T.NewObject("({})"));
  T.CheckFalse(T.NewObject("(/x/)"));
  T.CheckFalse(T.undefined());
  T.CheckFalse(T.null());
  T.CheckFalse(T.Val("x"));
  T.CheckFalse(T.Val(1));
}


TEST(IsRegExp) {
  FunctionTester T("(function(a) { return %_IsRegExp(a); })", flags);

  T.CheckFalse(T.NewObject("new Date()"));
  T.CheckFalse(T.NewObject("(function() {})"));
  T.CheckFalse(T.NewObject("([1])"));
  T.CheckFalse(T.NewObject("({})"));
  T.CheckTrue(T.NewObject("(/x/)"));
  T.CheckFalse(T.undefined());
  T.CheckFalse(T.null());
  T.CheckFalse(T.Val("x"));
  T.CheckFalse(T.Val(1));
}


TEST(IsSmi) {
  FunctionTester T("(function(a) { return %_IsSmi(a); })", flags);

  T.CheckFalse(T.NewObject("new Date()"));
  T.CheckFalse(T.NewObject("(function() {})"));
  T.CheckFalse(T.NewObject("([1])"));
  T.CheckFalse(T.NewObject("({})"));
  T.CheckFalse(T.NewObject("(/x/)"));
  T.CheckFalse(T.undefined());
  T.CheckTrue(T.Val(1));
  T.CheckFalse(T.Val(1.1));
  T.CheckFalse(T.Val(-0.0));
  T.CheckTrue(T.Val(-2));
  T.CheckFalse(T.Val(-2.3));
}


TEST(StringAdd) {
  FunctionTester T("(function(a,b) { return %_StringAdd(a,b); })", flags);

  T.CheckCall(T.Val("aaabbb"), T.Val("aaa"), T.Val("bbb"));
  T.CheckCall(T.Val("aaa"), T.Val("aaa"), T.Val(""));
  T.CheckCall(T.Val("bbb"), T.Val(""), T.Val("bbb"));
}


TEST(StringCharCodeAt) {
  FunctionTester T("(function(a,b) { return %_StringCharCodeAt(a,b); })",
                   flags);

  T.CheckCall(T.Val('e'), T.Val("huge fan!"), T.Val(3));
  T.CheckCall(T.Val('f'), T.Val("\xE2\x9D\x8A fan!"), T.Val(2));
  T.CheckCall(T.nan(), T.Val("not a fan!"), T.Val(23));
}


TEST(StringCompare) {
  FunctionTester T("(function(a,b) { return %_StringCompare(a,b); })", flags);

  T.CheckCall(T.Val(-1), T.Val("aaa"), T.Val("bbb"));
  T.CheckCall(T.Val(0.0), T.Val("bbb"), T.Val("bbb"));
  T.CheckCall(T.Val(+1), T.Val("ccc"), T.Val("bbb"));
}


TEST(SubString) {
  FunctionTester T("(function(a,b) { return %_SubString(a,b,b+3); })", flags);

  T.CheckCall(T.Val("aaa"), T.Val("aaabbb"), T.Val(0.0));
  T.CheckCall(T.Val("abb"), T.Val("aaabbb"), T.Val(2));
  T.CheckCall(T.Val("aaa"), T.Val("aaa"), T.Val(0.0));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
