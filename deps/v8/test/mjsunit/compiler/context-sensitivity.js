// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const object1 = {[Symbol.toPrimitive]() { return 1; }};
const thrower = {[Symbol.toPrimitive]() { throw new Error(); }};

// Test that JSAdd is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y + x);
  }

  assertEquals(1, foo(0));
  assertEquals(2, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(0));
  assertEquals(2, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSSubtract is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y - x);
  }

  assertEquals(1, foo(0));
  assertEquals(0, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(0));
  assertEquals(0, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSMultiply is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y * x);
  }

  assertEquals(1, foo(1));
  assertEquals(1, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(1));
  assertEquals(1, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSDivide is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y / x);
  }

  assertEquals(1, foo(1));
  assertEquals(1, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(1));
  assertEquals(1, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSModulus is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y % x);
  }

  assertEquals(0, foo(1));
  assertEquals(0, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(1));
  assertEquals(0, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSExponentiate is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y ** x);
  }

  assertEquals(1, foo(1));
  assertEquals(1, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(1));
  assertEquals(1, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSBitwiseOr is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y | x);
  }

  assertEquals(1, foo(1));
  assertEquals(1, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(1));
  assertEquals(1, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSBitwiseAnd is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y & x);
  }

  assertEquals(1, foo(1));
  assertEquals(1, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(1));
  assertEquals(1, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSBitwiseXor is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y ^ x);
  }

  assertEquals(0, foo(1));
  assertEquals(0, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(1));
  assertEquals(0, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSShiftLeft is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y << x);
  }

  assertEquals(2, foo(1));
  assertEquals(2, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo(1));
  assertEquals(2, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSShiftRight is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y >> x);
  }

  assertEquals(0, foo(1));
  assertEquals(0, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(1));
  assertEquals(0, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSShiftRightLogical is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y >>> x);
  }

  assertEquals(0, foo(1));
  assertEquals(0, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(1));
  assertEquals(0, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSEqual is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y == x);
  }

  assertFalse(foo(0));
  assertTrue(foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(0));
  assertTrue(foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSLessThan is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y < x);
  }

  assertFalse(foo(0));
  assertFalse(foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(0));
  assertFalse(foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSGreaterThan is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => x > y);
  }

  assertFalse(foo(0));
  assertFalse(foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(0));
  assertFalse(foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSLessThanOrEqual is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => y <= x);
  }

  assertFalse(foo(0));
  assertTrue(foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(0));
  assertTrue(foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSGreaterThanOrEqual is not context-sensitive.
(function() {
  function bar(fn) {
    return fn(1);
  }

  function foo(x) {
    return bar(y => x >= y);
  }

  assertFalse(foo(0));
  assertTrue(foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(0));
  assertTrue(foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSInstanceOf is not context-sensitive.
(function() {
  function bar(fn) {
    return fn({});
  }

  function foo(c) {
    return bar(o => o instanceof c);
  }

  assertTrue(foo(Object));
  assertFalse(foo(Array));
  assertThrows(() => foo({[Symbol.hasInstance]() { throw new Error(); }}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo(Object));
  assertFalse(foo(Array));
  assertThrows(() => foo({[Symbol.hasInstance]() { throw new Error(); }}));
})();

// Test that JSBitwiseNot is not context-sensitive.
(function() {
  function bar(fn) {
    return fn();
  }

  function foo(x) {
    return bar(() => ~x);
  }

  assertEquals(0, foo(-1));
  assertEquals(~1, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(-1));
  assertEquals(~1, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSNegate is not context-sensitive.
(function() {
  function bar(fn) {
    return fn();
  }

  function foo(x) {
    return bar(() => -x);
  }

  assertEquals(1, foo(-1));
  assertEquals(-1, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(-1));
  assertEquals(-1, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSIncrement is not context-sensitive.
(function() {
  function bar(fn) {
    return fn();
  }

  function foo(x) {
    return bar(() => ++x);
  }

  assertEquals(1, foo(0));
  assertEquals(2, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(0));
  assertEquals(2, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSDecrement is not context-sensitive.
(function() {
  function bar(fn) {
    return fn();
  }

  function foo(x) {
    return bar(() => --x);
  }

  assertEquals(1, foo(2));
  assertEquals(0, foo(object1));
  assertThrows(() => foo(thrower));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(2));
  assertEquals(0, foo(object1));
  assertThrows(() => foo(thrower));
})();

// Test that JSCreateArguments[UnmappedArguments] is not context-sensitive.
(function() {
  function bar(fn) {
    return fn();
  }

  function foo() {
    "use strict";
    return bar(() => arguments)[0];
  }

  assertEquals(0, foo(0, 1));
  assertEquals(1, foo(1, 2));
  assertEquals(undefined, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(0, 1));
  assertEquals(1, foo(1, 2));
  assertEquals(undefined, foo());
})();

// Test that JSCreateArguments[RestParameters] is not context-sensitive.
(function() {
  function bar(fn) {
    return fn();
  }

  function foo(...args) {
    return bar(() => args)[0];
  }

  assertEquals(0, foo(0, 1));
  assertEquals(1, foo(1, 2));
  assertEquals(undefined, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(0, 1));
  assertEquals(1, foo(1, 2));
  assertEquals(undefined, foo());
})();

// Test that JSLoadGlobal/JSStoreGlobal are not context-sensitive.
(function(global) {
  var actualValue = 'Some value';

  Object.defineProperty(global, 'globalValue', {
    configurable: true,
    enumerable: true,
    get: function() {
      return actualValue;
    },
    set: function(v) {
      actualValue = v;
    }
  });

  function bar(fn) {
    return fn();
  }

  function foo(v) {
    return bar(() => {
      const o = globalValue;
      globalValue = v;
      return o;
    });
  }

  assertEquals('Some value', foo('Another value'));
  assertEquals('Another value', actualValue);
  assertEquals('Another value', foo('Some value'));
  assertEquals('Some value', actualValue);
  %OptimizeFunctionOnNextCall(foo);
  assertEquals('Some value', foo('Another value'));
  assertEquals('Another value', actualValue);
  assertEquals('Another value', foo('Some value'));
  assertEquals('Some value', actualValue);
})(this);

// Test that for..in is not context-sensitive.
(function() {
  function bar(fn) {
    return fn();
  }

  function foo(o) {
    return bar(() => {
      var s = "";
      for (var k in o) { s += k; }
      return s;
    });
  }

  assertEquals('abc', foo({a: 1, b: 2, c: 3}));
  assertEquals('ab', foo(Object.create({a: 1, b: 2})));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals('abc', foo({a: 1, b: 2, c: 3}));
  assertEquals("ab", foo(Object.create({a:1, b:2})));
})();

// Test that most generator operations are not context-sensitive.
(function() {
  function bar(fn) {
    let s = undefined;
    for (const x of fn()) {
      if (s === undefined) s = x;
      else s += x;
    }
    return s;
  }

  function foo(x, y, z) {
    return bar(function*() {
      yield x;
      yield y;
      yield z;
    });
  }

  assertEquals(6, foo(1, 2, 3));
  assertEquals("abc", foo("a", "b", "c"));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(6, foo(1, 2, 3));
  assertEquals("abc", foo("a", "b", "c"));
})();
