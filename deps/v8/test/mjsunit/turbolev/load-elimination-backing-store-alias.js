// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling

// With loop peeling disabled, in this test, Maglev isn't able to load-eliminate
// `o.x` in and after the loop because it assumes that the loop could contain
// side-effects.
// Turboshaft's LateLoadElimination can figure this out, but for this it needs
// to realized that `arr[i] = 42` cannot alias with `o.x`. This requires an
// AssumeMap when loading the elements array of `arr` because Maglev just
// "knows" that the map of the elements array is and that it doesn't alias with
// `o.x` but this information isn't propagated to Turbosahft's
// LateLoadElimination (unless we insert an AssumeMap).

function foo(o, arr) {
  let v1 = +o.x;

  for (let i = 0; i < o.x; i++) {
    %TurbofanStaticAssert(v1 == o.x);
    arr[i] = 42;
  }

  %TurbofanStaticAssert(v1 == o.x);
}

let arr = [1, 2, 3, 4, 5, 6];
arr[0] = 42; // Making {arr} non-const and non-COW.
let o = { x : 5 };
o.x++; // Making o.x non-const.

%PrepareFunctionForOptimization(foo);
foo(o, arr);
foo(o, arr);

%OptimizeFunctionOnNextCall(foo);
foo(o, arr);
