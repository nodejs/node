// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --allow-natives-syntax --turbofan
// Flags: --max-turbolev-eager-inlined-bytecode-size=0
// Flags: --turbolev-escape-analysis

function baz(deopt) {
  if (deopt) {
    bar.arguments[0][3] += 10;
  }
}
%NeverOptimizeFunction(baz);

function bar(arr, deopt) {
  baz(deopt);
}

function get_const() {
  return 3;
}

function foo(x, deopt) {
  let arr = [0, 2, 4, x, 16, 32];
  %AssertEscapeAnalysisElided(arr);
  let index = get_const();
  bar(arr, deopt);
  // The following load has a seemingly non-constant index, but after inlining
  // and constant-folding has happened, it should have a constant index and thus
  // it shouldn't prevent escape analysis.
  let ret = arr[index];
  return ret;
}

%PrepareFunctionForOptimization(get_const);
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(8, foo(8, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(8, foo(8, false));
assertOptimized(foo);

// Triggering deopt.
assertEquals(8+10, foo(8, true));
assertUnoptimized(foo);
