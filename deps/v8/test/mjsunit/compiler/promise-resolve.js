// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function assertFulfilledWith(expected, thenable) {
  assertPromiseResult(thenable, v => assertEquals(expected, v));
}

(function() {
  function foo() { return Promise.resolve(); }
  assertFulfilledWith(undefined, foo());
  assertFulfilledWith(undefined, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFulfilledWith(undefined, foo());
})();

(function() {
  function foo(x) { return Promise.resolve(x); }
  assertFulfilledWith(3, foo(3));
  assertFulfilledWith(3, foo(3));
  %OptimizeFunctionOnNextCall(foo);
  assertFulfilledWith(3, foo(3));
})();

(function() {
  function foo(x, y) { return Promise.resolve(x, y); }
  assertFulfilledWith(1, foo(1, 0));
  assertFulfilledWith(2, foo(2, 1));
  %OptimizeFunctionOnNextCall(foo);
  assertFulfilledWith(3, foo(3, 2));
})();

(function() {
  function foo(x) { return Promise.resolve({x}); }
  assertFulfilledWith({x:1}, foo(1));
  assertFulfilledWith({x:2}, foo(2));
  %OptimizeFunctionOnNextCall(foo);
  assertFulfilledWith({x:3}, foo(3));
})();

(function() {
  function foo(x) { return Promise.resolve(Promise.resolve(x)); }
  assertFulfilledWith(null, foo(null));
  assertFulfilledWith('a', foo('a'));
  %OptimizeFunctionOnNextCall(foo);
  assertFulfilledWith(42, foo(42));
})();

(function() {
  const thenable = new class Thenable {
    then(fulfill, reject) {
      fulfill(1);
    }
  };
  function foo() { return Promise.resolve(thenable); }
  assertFulfilledWith(1, foo());
  assertFulfilledWith(1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFulfilledWith(1, foo());
})();

(function() {
  const MyPromise = class MyPromise extends Promise {};

  (function() {
    function foo() { return MyPromise.resolve(); }
    assertFulfilledWith(undefined, foo());
    assertFulfilledWith(undefined, foo());
    %OptimizeFunctionOnNextCall(foo);
    assertFulfilledWith(undefined, foo());
  })();

  (function() {
    function foo(x) { return MyPromise.resolve(x); }
    assertFulfilledWith(3, foo(3));
    assertFulfilledWith(3, foo(3));
    %OptimizeFunctionOnNextCall(foo);
    assertFulfilledWith(3, foo(3));
  })();

  (function() {
    function foo(x, y) { return MyPromise.resolve(x, y); }
    assertFulfilledWith(1, foo(1, 0));
    assertFulfilledWith(2, foo(2, 1));
    %OptimizeFunctionOnNextCall(foo);
    assertFulfilledWith(3, foo(3, 2));
  })();

  (function() {
    function foo(x) { return MyPromise.resolve({x}); }
    assertFulfilledWith({x:1}, foo(1));
    assertFulfilledWith({x:2}, foo(2));
    %OptimizeFunctionOnNextCall(foo);
    assertFulfilledWith({x:3}, foo(3));
  })();

  (function() {
    function foo(x) { return MyPromise.resolve(Promise.resolve(x)); }
    assertFulfilledWith(null, foo(null));
    assertFulfilledWith('a', foo('a'));
    %OptimizeFunctionOnNextCall(foo);
    assertFulfilledWith(42, foo(42));
  })();

  (function() {
    const thenable = new class Thenable {
      then(fulfill, reject) {
        fulfill(1);
      }
    };
    function foo() { return MyPromise.resolve(thenable); }
    assertFulfilledWith(1, foo());
    assertFulfilledWith(1, foo());
    %OptimizeFunctionOnNextCall(foo);
    assertFulfilledWith(1, foo());
  })();
})();
