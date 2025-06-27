// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-externalize-string

(function() {
  function foo(s) {
    return "abcdefghijklm" + s;
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(isOneByteString(foo("0")));
  assertTrue(isOneByteString(foo("0")));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(isOneByteString(foo("0")));
})();

(function() {
  function foo(s) {
    return s + "abcdefghijklm";
  }

  %PrepareFunctionForOptimization(foo);
  assertTrue(isOneByteString(foo("0")));
  assertTrue(isOneByteString(foo("0")));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(isOneByteString(foo("0")));
})();

(function() {
  function foo(s) {
    return "abcdefghijklm" + s;
  }

  %PrepareFunctionForOptimization(foo);
  assertFalse(isOneByteString(foo("\u1234")));
  assertFalse(isOneByteString(foo("\u1234")));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(isOneByteString(foo("\u1234")));
})();

(function() {
  function foo(s) {
    return s + "abcdefghijklm";
  }

  %PrepareFunctionForOptimization(foo);
  assertFalse(isOneByteString(foo("\u1234")));
  assertFalse(isOneByteString(foo("\u1234")));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(isOneByteString(foo("\u1234")));
})();

(function() {
  function foo(s) {
    return "abcdefghijkl\u1234" + s;
  }

  %PrepareFunctionForOptimization(foo);
  assertFalse(isOneByteString(foo("0")));
  assertFalse(isOneByteString(foo("0")));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(isOneByteString(foo("0")));
})();

(function() {
  function foo(s) {
    return s + "abcdefghijkl\u1234";
  }

  %PrepareFunctionForOptimization(foo);
  assertFalse(isOneByteString(foo("0")));
  assertFalse(isOneByteString(foo("0")));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(isOneByteString(foo("0")));
})();
