// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

// The reduced RegExp.prototype.test has to notice a replaced
// RegExp.prototype.exec, both in code that is already optimized and when
// optimizing afterwards. The call goes through a constant so that no load
// from RegExp.prototype guards the optimized code.

const test = RegExp.prototype.test;

function optimize(f) {
  %PrepareFunctionForOptimization(f);
  f("a");
  f("b");
  %OptimizeFunctionOnNextCall(f);
}

function f(s) { return test.call(/a/, s); }
optimize(f);
assertTrue(f("a"));
assertFalse(f("b"));

let calls = 0;
RegExp.prototype.exec = function(s) { calls++; return null; };
assertFalse(f("a"));
assertEquals(1, calls);

optimize(f);
assertFalse(f("a"));
assertEquals(4, calls);
