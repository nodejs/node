// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestRegularFunction() {
  function f(a) {
    a.stack = "boom";
  }
  %PrepareFunctionForOptimization(f);
  Error.captureStackTrace(f);
  f(f);
})();

(function TestFunctionConstructor() {
  function f(a) {
    a.stack = "boom";
  }
  %PrepareFunctionForOptimization(f);
  Error.captureStackTrace(Function);
  f(Function);
})();

(function TestArrowFunction() {
  function f(a) {
    a.stack = "boom";
  }
  %PrepareFunctionForOptimization(f);

  const arrow = a => {};

  Error.captureStackTrace(arrow);
  f(arrow);
})();

(function TestClass() {
  function f(a) {
    a.stack = "boom";
  }
  %PrepareFunctionForOptimization(f);

  class A {};

  Error.captureStackTrace(A);
  f(A);
})();

(function TestStaticMethod() {
  function f(a) {
    a.stack = "boom";
  }
  %PrepareFunctionForOptimization(f);

  class A {
    static m() {}
  };

  Error.captureStackTrace(A.m);
  f(A.m);
})();
