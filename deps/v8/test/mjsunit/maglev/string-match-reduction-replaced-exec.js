// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

// The reduced String.prototype.match has to notice a replaced
// RegExp.prototype.exec, both in code that is already optimized and when
// optimizing afterwards.

function optimize(f) {
  %PrepareFunctionForOptimization(f);
  f("a");
  f("b");
  %OptimizeFunctionOnNextCall(f);
}

function f(s) { return s.match(/^a/); }
optimize(f);
assertEquals(["a"], [...f("a")]);
assertNull(f("b"));

let calls = 0;
RegExp.prototype.exec = function(s) { calls++; return null; };
assertNull(f("b"));
assertEquals(1, calls);
assertNull(f("a"));
assertEquals(2, calls);

optimize(f);
assertNull(f("b"));
assertNull(f("a"));
assertEquals(6, calls);
