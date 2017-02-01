// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function A() {}
var a = new A();

var B = {
  [Symbol.hasInstance](o) {
    return false;
  }
};
%ToFastProperties(B.__proto__);

var C = Object.create({
  [Symbol.hasInstance](o) {
    return true;
  }
});
%ToFastProperties(C.__proto__);

var D = Object.create({
  [Symbol.hasInstance](o) {
    return o === a;
  }
});
%ToFastProperties(D.__proto__);

var E = Object.create({
  [Symbol.hasInstance](o) {
    if (o === a) throw o;
    return true;
  }
});
%ToFastProperties(E.__proto__);

function F() {}
F.__proto__ = null;

(function() {
  function foo(o) { return o instanceof A; }

  assertTrue(foo(a));
  assertTrue(foo(a));
  assertTrue(foo(new A()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(a));
  assertTrue(foo(new A()));
})();

(function() {
  function foo(o) {
    try {
      return o instanceof A;
    } catch (e) {
      return e;
    }
  }

  assertTrue(foo(a));
  assertTrue(foo(a));
  assertTrue(foo(new A()));
  assertEquals(1, foo(new Proxy({}, {getPrototypeOf() { throw 1; }})));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(a));
  assertTrue(foo(new A()));
  assertEquals(1, foo(new Proxy({}, {getPrototypeOf() { throw 1; }})));
})();

(function() {
  function foo(o) { return o instanceof B; }

  assertFalse(foo(a));
  assertFalse(foo(a));
  assertFalse(foo(new A()));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(a));
  assertFalse(foo(new A()));
})();

(function() {
  function foo(o) { return o instanceof C; }

  assertTrue(foo(a));
  assertTrue(foo(a));
  assertTrue(foo(new A()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(a));
  assertTrue(foo(new A()));
})();

(function() {
  function foo(o) { return o instanceof D; }

  assertTrue(foo(a));
  assertTrue(foo(a));
  assertFalse(foo(new A()));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(a));
  assertFalse(foo(new A()));
})();

(function() {
  function foo(o) {
    try {
      return o instanceof E;
    } catch (e) {
      return false;
    }
  }

  assertFalse(foo(a));
  assertTrue(foo(new A()));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(a));
  assertTrue(foo(new A()));
})();

(function() {
  function foo(o) {
    return o instanceof F;
  }

  assertFalse(foo(a));
  assertFalse(foo(new A()));
  assertTrue(foo(new F()));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(a));
  assertFalse(foo(new A()));
  assertTrue(foo(new F()));
})();
