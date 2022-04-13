// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


(function testFunctionPrototypeHasInstance() {
  class A {};
  var a = new A;

  function foo() {
    return A[Symbol.hasInstance](a);
  };

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();


(function testFunctionPrototypeHasInstanceWithInference() {
  class A {};
  var a = new A;
  a.bla = 42;

  function foo() {
    a.bla;
    return A[Symbol.hasInstance](a);
  };

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();


(function testFunctionPrototypeHasInstanceWithBoundFunction() {
  class A {};
  var a = new A;
  var f = A.bind({});

  function foo() {
    return f[Symbol.hasInstance](a);
  };

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());

  // JSCallReducer::ReduceFunctionPrototypeHasInstance ->
  // JSNative...::ReduceJSOrdinaryHasInstance ->
  // JSNative...::ReduceJSInstanceOf (on bound_target_function)
  // ~~~~~>
  // JSCallReducer::ReduceFunctionPrototypeHasInstance
  // JSNative...::ReduceJSOrdinaryHasInstance ->
  // JSNative...::ReduceJSHasInPrototypeChain
})();


(function testSimpleInstanceOf() {
  class A {};
  var a = new A;

  function foo() {
    return a instanceof A;
  };

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();
