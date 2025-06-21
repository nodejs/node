// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite("ArrowFunctionShort", [1000], [
  new Benchmark("ArrowFunctionShort", false, true, iterations, Run, ArrowFunctionShortSetup)
]);

new BenchmarkSuite("ArrowFunctionLong", [1000], [
  new Benchmark("ArrowFunctionLong", false, true, iterations, Run, ArrowFunctionLongSetup)
]);

new BenchmarkSuite("CommaSepExpressionListShort", [1000], [
  new Benchmark("CommaSepExpressionListShort", false, true, iterations, Run, CommaSepExpressionListShortSetup)
]);

new BenchmarkSuite("CommaSepExpressionListLong", [1000], [
  new Benchmark("CommaSepExpressionListLong", false, true, iterations, Run, CommaSepExpressionListLongSetup)
]);

new BenchmarkSuite("CommaSepExpressionListLate", [1000], [
  new Benchmark("CommaSepExpressionListLate", false, true, iterations, Run, CommaSepExpressionListLateSetup)
]);

new BenchmarkSuite("FakeArrowFunction", [1000], [
  new Benchmark("FakeArrowFunction", false, true, iterations, Run, FakeArrowFunctionSetup)
]);

function ArrowFunctionShortSetup() {
  code = "let a;\n" + "a = (a,b) => { return a+b; }\n".repeat(50)
}

function ArrowFunctionLongSetup() {
  code = "let a;\n" + "a = (a,b,c,d,e,f,g,h,i,j) => { return a+b; }\n".repeat(50)
}

function CommaSepExpressionListShortSetup() {
  code = "let a;\n" + "a = (a,1)\n".repeat(50)
}

function CommaSepExpressionListLongSetup() {
  code = "let a; let b; let c;\n" + "a = (a,2,3,4,5,b,c,1,7,1)\n".repeat(50)
}

function CommaSepExpressionListLateSetup() {
  code = "let a; let b; let c; let d; let e; let f; let g; let h; let i;\n"
    + "a = (a,b,c,d,e,f,g,h,i,1)\n".repeat(50)
}

function FakeArrowFunctionSetup() {
  code = "let a; let b; let c; let d; let e; let f; let g; let h; let i; let j;\n"
    + "a = (a,b,c,d,e,f,g,h,i,j)\n".repeat(50)
}

function Run() {
  if (code == undefined) {
    throw new Error("No test data");
  }
  eval(code);
}
