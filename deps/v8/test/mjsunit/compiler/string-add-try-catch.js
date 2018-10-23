// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test that string concatenation overflow (going over string max length)
// is handled gracefully, i.e. an error is thrown

var a = "a".repeat(%StringMaxLength());

(function() {
  function foo(a, b) {
    try {
      return a + "0123456789012";
    } catch (e) {
      return e;
    }
  }

  foo("a");
  foo("a");
  %OptimizeFunctionOnNextCall(foo);
  foo("a");
  assertInstanceof(foo(a), RangeError);
})();

(function() {
  function foo(a, b) {
    try {
      return "0123456789012" + a;
    } catch (e) {
      return e;
    }
  }

  foo("a");
  foo("a");
  %OptimizeFunctionOnNextCall(foo);
  foo("a");
  assertInstanceof(foo(a), RangeError);
})();

(function() {
  function foo(a, b) {
    try {
      return "0123456789012".concat(a);
    } catch (e) {
      return e;
    }
  }

  foo("a");
  foo("a");
  %OptimizeFunctionOnNextCall(foo);
  foo("a");
  assertInstanceof(foo(a), RangeError);
})();

var obj = {
  toString: function() {
    throw new Error('toString has thrown');
  }
};

(function() {
  function foo(a, b) {
    try {
      return "0123456789012" + obj;
    } catch (e) {
      return e;
    }
  }

  foo("a");
  foo("a");
  %OptimizeFunctionOnNextCall(foo);
  foo("a");
  assertInstanceof(foo(a), Error);
})();

(function() {
  function foo(a, b) {
    try {
      return a + 123;
    } catch (e) {
      return e;
    }
  }

  foo("a");
  foo("a");
  %OptimizeFunctionOnNextCall(foo);
  foo("a");
  assertInstanceof(foo(a), RangeError);
})();
