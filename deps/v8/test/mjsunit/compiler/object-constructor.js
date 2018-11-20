// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Common pattern in Webpack 3 generated bundles, see
// https://github.com/webpack/webpack/issues/5600 for details.
(function ObjectConstructorWithKnownFunction() {
  "use strict";
  class A {
    bar() { return this; }
  };
  function foo(a) {
    return Object(a.bar)();
  }
  assertEquals(undefined, foo(new A));
  assertEquals(undefined, foo(new A));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo(new A));
})();

(function ObjectConstructorWithString() {
  "use strict";
  function foo() {
    return Object("a");
  }
  assertEquals('object', typeof foo());
  assertEquals('object', typeof foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals('object', typeof foo());
})();

// Object constructor subclassing via Class Factories, see
// https://twitter.com/FremyCompany/status/905977048006402048
// for the hint.
(function ObjectConstructorSubClassing() {
  "use strict";
  const Factory = Base => class A extends Base {};
  const A = Factory(Object);
  function foo() {
    return new A(1, 2, 3);
  }
  assertInstanceof(foo(), A);
  assertInstanceof(foo(), Object);
  assertInstanceof(foo(), A);
  assertInstanceof(foo(), Object);
  %OptimizeFunctionOnNextCall(foo);
  assertInstanceof(foo(), A);
  assertInstanceof(foo(), Object);
})();
