// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev --no-force-slow-path

// The fast regexp path of String.prototype.match requires an intact RegExp
// species protector, so the reduction to it does too.

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
assertOptimized(f);

Object.defineProperty(RegExp, Symbol.species, { value: undefined });
assertUnoptimized(f);
assertEquals(["a"], [...f("a")]);
assertNull(f("b"));

optimize(f);
assertEquals(["a"], [...f("a")]);
assertNull(f("b"));
