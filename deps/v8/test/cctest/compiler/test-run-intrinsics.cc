// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/optimized-compilation-info.h"
#include "src/objects/string.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

uint32_t flags = OptimizedCompilationInfo::kInliningEnabled;

TEST(Call) {
  FunctionTester T("(function(a,b) { return %_Call(b, a, 1, 2, 3); })", flags);
  CompileRun("function f(a,b,c) { return a + b + c + this.d; }");

  T.CheckCall(T.Val(129), T.NewObject("({d:123})"), T.NewObject("f"));
  T.CheckCall(T.Val("6x"), T.NewObject("({d:'x'})"), T.NewObject("f"));
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8
