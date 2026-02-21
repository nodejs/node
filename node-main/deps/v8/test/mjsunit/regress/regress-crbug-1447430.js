// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var s = %CreatePrivateSymbol('x');

(function TestSmi() {
  function f(o, p) {
    o[p] = 153;
  }
  (1).__proto__[s] = 42;
  %PrepareFunctionForOptimization(f);
  assertEquals(f(42, s), undefined);
}());

(function TestString() {
  function f(o, p) {
    o[p] = 153;
  }
  ('xyz').__proto__[s] = 42;
  %PrepareFunctionForOptimization(f);
  assertEquals(f('abc', s), undefined);
}());

(function TestSymbol() {
  function f(o, p) {
    o[p] = 153;
  }
  Symbol('xyz').__proto__[s] = 42;
  %PrepareFunctionForOptimization(f);
  assertEquals(f(Symbol('abc'), s), undefined);
}());
