// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

// Test using a builtin continuation to make sure that the frame state is
// correctly updated.

let sum = 0;
let deopt = false;
deopt = true; // breaking constness
deopt = false;

function baz(arg_deopt) {
  if (arg_deopt && deopt) {
    inner.arguments[0].x += 17;
  }
}
%NeverOptimizeFunction(baz);

function inner(o) {
  baz(o.x == 3);
}

function bar(e) {
  let o = { x : e };
  inner(o);
  o.x += 3;
  %AssertEscapeAnalysisElided(o);
  sum += o.x;
}

function foo(arr, deopt) {
  arr.forEach(bar);
}

%PrepareFunctionForOptimization(inner);
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
sum = 0;
foo([1, 2, 3, 4], false);
assertEquals(22, sum);
sum = 0;
foo([1, 2, 3, 4], false);
assertEquals(22, sum);


%OptimizeFunctionOnNextCall(foo);
sum = 0;
foo([1, 2, 3, 4], false);
assertEquals(22, sum);
assertOptimized(foo);

deopt = true;
assertOptimized(foo); // No constness-dependency related deopt
sum = 0;
foo([1, 2, 3, 4], false);
assertEquals(39, sum);
assertUnoptimized(foo);
