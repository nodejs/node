// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Test cases for Array constructor inlining with double elements kinds.
//
// Note the order of assertions below matters. `assertEquals` may change the
// elements kind, and thus must come after all other assertions.

function assertDoubleElementsKind(obj) {
  assertTrue(%HasDoubleElements(obj));
}

function assertSmiElementsKind(obj) {
  assertTrue(%HasSmiElements(obj));
}

function assertObjectElementsKind(obj) {
  assertTrue(%HasObjectElements(obj));
}

// Test with all Smi values
(() => {
  function foo() { return new Array(1, 2, 3); }

  %PrepareFunctionForOptimization(foo);
  let result = foo();
  assertSmiElementsKind(result);
  %OptimizeFunctionOnNextCall(foo);
  result = foo();
  assertSmiElementsKind(result);
  assertOptimized(foo);
  assertEquals([1, 2, 3], result);
})();

// Test with all Double values
(() => {
  function foo() { return new Array(1.1, 2.2, 3.3); }

  %PrepareFunctionForOptimization(foo);
  let result = foo();
  assertDoubleElementsKind(result);
  %OptimizeFunctionOnNextCall(foo);
  result = foo();
  assertDoubleElementsKind(result);
  assertOptimized(foo);
  assertEquals([1.1, 2.2, 3.3], result);
})();

// Test with mixed Smi and Double values
(() => {
  function foo() { return new Array(1, 2.2, 3); }

  %PrepareFunctionForOptimization(foo);
  let result = foo();
  assertDoubleElementsKind(result);
  %OptimizeFunctionOnNextCall(foo);
  result = foo();
  assertDoubleElementsKind(result);
  assertOptimized(foo);
  assertEquals([1, 2.2, 3], result);
})();

// Test with a non-number value
(() => {
  function foo() { return new Array(1.1, "hello", 3.3); }

  %PrepareFunctionForOptimization(foo);
  let result = foo();
  assertObjectElementsKind(result);
  %OptimizeFunctionOnNextCall(foo);
  result = foo();
  assertObjectElementsKind(result);
  assertOptimized(foo);
  assertEquals([1.1, "hello", 3.3], result);
})();

// --- Speculative cases with deopt ---

// Speculate PACKED_DOUBLE_ELEMENTS, deopt because of non-number
(() => {
  function foo(deopt) {
    let a = 1.1;
    let b = 2.2;
    let c = 3.3;
    if (deopt) {
      b = "hello";
    }
    return new Array(a, b, c);
  }

  %PrepareFunctionForOptimization(foo);
  let result = foo(false);
  assertDoubleElementsKind(result);
  %OptimizeFunctionOnNextCall(foo);
  result = foo(false);
  assertDoubleElementsKind(result);
  assertOptimized(foo);

  // Deopt
  result = foo(true);
  assertObjectElementsKind(result);
  assertUnoptimized(foo);
  assertEquals([1.1, "hello", 3.3], result);

  // Reoptimization will pick up the updated PACKED_ELEMENTS kind.
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  result = foo(true);
  assertObjectElementsKind(result);
  assertOptimized(foo);
})();
