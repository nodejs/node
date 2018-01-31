// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  function foo(s) { return s.slice(-1); }

  assertEquals('', foo(''));
  assertEquals('a', foo('a'));
  assertEquals('b', foo('ab'));
  assertEquals('c', foo('abc'));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals('', foo(''));
  assertEquals('a', foo('a'));
  assertEquals('b', foo('ab'));
  assertEquals('c', foo('abc'));
})();

(function() {
  function foo(s) { return s.slice(-1, undefined); }

  assertEquals('', foo(''));
  assertEquals('a', foo('a'));
  assertEquals('b', foo('ab'));
  assertEquals('c', foo('abc'));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals('', foo(''));
  assertEquals('a', foo('a'));
  assertEquals('b', foo('ab'));
  assertEquals('c', foo('abc'));
})();
