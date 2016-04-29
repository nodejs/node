// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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


TEST(OneByteSeqStringGetChar) {
  FunctionTester T("(function(a,b) { return %_OneByteSeqStringGetChar(a,b); })",
                   flags);

  Handle<SeqOneByteString> string =
      T.main_isolate()->factory()->NewRawOneByteString(3).ToHandleChecked();
  string->SeqOneByteStringSet(0, 'b');
  string->SeqOneByteStringSet(1, 'a');
  string->SeqOneByteStringSet(2, 'r');
  T.CheckCall(T.Val('b'), string, T.Val(0.0));
  T.CheckCall(T.Val('a'), string, T.Val(1));
  T.CheckCall(T.Val('r'), string, T.Val(2));
}


TEST(OneByteSeqStringSetChar) {
  FunctionTester T("(function(a,b) { %_OneByteSeqStringSetChar(a,88,b); })",
                   flags);

  Handle<SeqOneByteString> string =
      T.main_isolate()->factory()->NewRawOneByteString(3).ToHandleChecked();
  string->SeqOneByteStringSet(0, 'b');
  string->SeqOneByteStringSet(1, 'a');
  string->SeqOneByteStringSet(2, 'r');
  T.Call(T.Val(1), string);
  CHECK_EQ('b', string->SeqOneByteStringGet(0));
  CHECK_EQ('X', string->SeqOneByteStringGet(1));
  CHECK_EQ('r', string->SeqOneByteStringGet(2));
}


TEST(StringAdd) {
  FunctionTester T("(function(a,b) { return %_StringAdd(a,b); })", flags);

  T.CheckCall(T.Val("aaabbb"), T.Val("aaa"), T.Val("bbb"));
  T.CheckCall(T.Val("aaa"), T.Val("aaa"), T.Val(""));
  T.CheckCall(T.Val("bbb"), T.Val(""), T.Val("bbb"));
}


TEST(StringCharAt) {
  FunctionTester T("(function(a,b) { return %_StringCharAt(a,b); })", flags);

  T.CheckCall(T.Val("e"), T.Val("huge fan!"), T.Val(3));
  T.CheckCall(T.Val("f"), T.Val("\xE2\x9D\x8A fan!"), T.Val(2));
  T.CheckCall(T.Val(""), T.Val("not a fan!"), T.Val(23));
}


TEST(StringCharCodeAt) {
  FunctionTester T("(function(a,b) { return %_StringCharCodeAt(a,b); })",
                   flags);

  T.CheckCall(T.Val('e'), T.Val("huge fan!"), T.Val(3));
  T.CheckCall(T.Val('f'), T.Val("\xE2\x9D\x8A fan!"), T.Val(2));
  T.CheckCall(T.nan(), T.Val("not a fan!"), T.Val(23));
}


TEST(StringCharFromCode) {
  FunctionTester T("(function(a) { return %_StringCharFromCode(a); })", flags);

  T.CheckCall(T.Val("a"), T.Val(97));
  T.CheckCall(T.Val("\xE2\x9D\x8A"), T.Val(0x274A));
  T.CheckCall(T.Val(""), T.undefined());
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


TEST(TwoByteSeqStringGetChar) {
  FunctionTester T("(function(a,b) { return %_TwoByteSeqStringGetChar(a,b); })",
                   flags);

  Handle<SeqTwoByteString> string =
      T.main_isolate()->factory()->NewRawTwoByteString(3).ToHandleChecked();
  string->SeqTwoByteStringSet(0, 'b');
  string->SeqTwoByteStringSet(1, 'a');
  string->SeqTwoByteStringSet(2, 'r');
  T.CheckCall(T.Val('b'), string, T.Val(0.0));
  T.CheckCall(T.Val('a'), string, T.Val(1));
  T.CheckCall(T.Val('r'), string, T.Val(2));
}


TEST(TwoByteSeqStringSetChar) {
  FunctionTester T("(function(a,b) { %_TwoByteSeqStringSetChar(a,88,b); })",
                   flags);

  Handle<SeqTwoByteString> string =
      T.main_isolate()->factory()->NewRawTwoByteString(3).ToHandleChecked();
  string->SeqTwoByteStringSet(0, 'b');
  string->SeqTwoByteStringSet(1, 'a');
  string->SeqTwoByteStringSet(2, 'r');
  T.Call(T.Val(1), string);
  CHECK_EQ('b', string->SeqTwoByteStringGet(0));
  CHECK_EQ('X', string->SeqTwoByteStringGet(1));
  CHECK_EQ('r', string->SeqTwoByteStringGet(2));
}


TEST(ValueOf) {
  FunctionTester T("(function(a) { return %_ValueOf(a); })", flags);

  T.CheckCall(T.Val("a"), T.Val("a"));
  T.CheckCall(T.Val("b"), T.NewObject("(new String('b'))"));
  T.CheckCall(T.Val(123), T.Val(123));
  T.CheckCall(T.Val(456), T.NewObject("(new Number(456))"));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
