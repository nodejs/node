// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan --max-turbolev-eager-inlined-bytecode-size=0

function A(x) { this.x = x }

function load(o) {
  return o.x;
}

let old_A = new A(5);
old_A.x = 15;

    %PrepareFunctionForOptimization(load);
// Deprecating the original map for `A`.
(new A(1)).x = 4.25;
// Recording feedback with the original map for `A`.
load(old_A);
// Making the map unstable to force Maglev to clear it from the KNA on loop
// headers.
(new A(1)).y = 10;

function foo(deopt) {
  let o = new A(42);
  %AssertEscapeAnalysisElided(o);

  if (deopt) {
    o.x; // No feedback ==> unconditional deopt.
    return o;
  }

  // Empty loop to force Maglev to clear {o}'s map from the KNA.
  for (let i = 0; i < 42; i++) {}

  // The {o.x} inside of {load} will insert a CheckMapsWithMigration since the
  // map in its feedback is a migration target.
  return load(o);
}

%PrepareFunctionForOptimization(A);
%PrepareFunctionForOptimization(foo);
assertEquals(42, foo(false));
assertEquals(42, foo(false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(42, foo(false));
assertOptimized(foo);

// Triggering deopt.
assertEquals(new A(42), foo(true));
assertUnoptimized(foo);
