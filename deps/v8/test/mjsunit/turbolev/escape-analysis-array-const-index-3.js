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

let glob_obj = { x : 42 }
glob_obj.x = 17; // Non-const Smi field.

function foo(x, deopt) {
  let arr = [0, 2, 4, x, 16, 32];
  %AssertEscapeAnalysisElided(arr);
  let index = get_const();
  bar(arr, deopt);

  // Storing {index} to a Smi field so that we'll insert an UnsafeSmiUntag when
  // using it as an array index rather than a CheckedSmiUntag or a
  // CheckedObjectToIndex.
  glob_obj.x = index;
  return arr[index];
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
