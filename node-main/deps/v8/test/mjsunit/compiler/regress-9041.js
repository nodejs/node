// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  class A {};

  function foo(a, fn) {
    const C = a.constructor;
    fn(a);
    return a instanceof C;
  };

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo(new A(), a => {}));
  assertTrue(foo(new A(), a => {}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(new A(), a => {}));
  assertFalse(foo(new A(), a => { a.__proto__ = {}; }));
})();

(function() {
  class A {};
  A.__proto__ = {};
  A.prototype = {};

  function foo() {
    var x = Object.create(Object.create(Object.create(A.prototype)));
    return x instanceof A;
  };

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();

(function() {
  class A {};
  A.prototype = {};
  A.__proto__ = {};
  var a = {__proto__: new A, gaga: 42};

  function foo() {
    A.bla;  // Make A.__proto__ fast again.
    a.gaga;
    return a instanceof A;
  };

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();

(function() {
  class A {};
  A.prototype = {};
  A.__proto__ = {};
  const boundA = Function.prototype.bind.call(A, {});
  boundA.prototype = {};
  boundA.__proto__ = {};
  var a = {__proto__: new boundA, gaga: 42};

  function foo() {
    A.bla;  // Make A.__proto__ fast again.
    boundA.bla;  // Make boundA.__proto__ fast again.
    a.gaga;
    return a instanceof boundA;
  };

  %PrepareFunctionForOptimization(foo);
  assertTrue(foo());
  assertTrue(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo());
})();
