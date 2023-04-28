// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

// Make sure we inline the callback, pick up all possible TurboFan
// optimizations.
function RunOptFastReduceRight(value) {
  // Use of variable {value} in the callback function forces
  // context creation without escape analysis.
  //
  // Also, the arrow function requires inlining based on
  // SharedFunctionInfo.
  result = array.reduceRight((p, v, i, a) => p + value);
}

// Don't optimize because I want to optimize RunOptFastMap with a parameter
// to be used in the callback.
%NeverOptimizeFunction(OptFastReduceRight);
function OptFastReduceRight() { RunOptFastReduceRight("3"); }

function side_effect(a) { return a; }
%NeverOptimizeFunction(side_effect);
function OptUnreliableReduceRight() {
  result = array.reduceRight(func, side_effect(array));
}

DefineHigherOrderTests([
  // name, test function, setup function, user callback
  [
    'DoubleReduceRight', newClosure('reduceRight'), DoubleSetup,
    (p, v, i, o) => p + v
  ],
  [
    'SmiReduceRight', newClosure('reduceRight'), SmiSetup, (p, v, i, a) => p + 1
  ],
  [
    'FastReduceRight', newClosure('reduceRight'), FastSetup,
    (p, v, i, a) => p + v
  ],
  ['OptFastReduceRight', OptFastReduceRight, FastSetup, undefined],
  [
    'OptUnreliableReduceRight', OptUnreliableReduceRight, FastSetup,
    (p, v, i, a) => p + v
  ]
]);

})();
