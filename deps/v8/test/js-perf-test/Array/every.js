// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

// Make sure we inline the callback, pick up all possible TurboFan
// optimizations.
function RunOptFastEvery(multiple) {
  // Use of variable multiple in the callback function forces
  // context creation without escape analysis.
  //
  // Also, the arrow function requires inlining based on
  // SharedFunctionInfo.
  result = array.every((v, i, a) => multiple === 3);
}

// Don't optimize because I want to optimize RunOptFastMap with a parameter
// to be used in the callback.
%NeverOptimizeFunction(OptFastEvery);
function OptFastEvery() { RunOptFastEvery(3); }

function side_effect(a) { return a; }
%NeverOptimizeFunction(side_effect);
function OptUnreliableEvery() {
  result = array.every(func, side_effect(array));
}

DefineHigherOrderTests([
  // name, test function, setup function, user callback
  ['DoubleEvery', newClosure('every'), DoubleSetup, v => v > 0.0],
  ['SmiEvery', newClosure('every'), SmiSetup, v => v != 34343],
  ['FastEvery', newClosure('every'), FastSetup, v => v !== 'hi'],
  ['OptFastEvery', OptFastEvery, FastSetup, v => true],
  ['OptUnreliableEvery', OptUnreliableEvery, FastSetup, v => true]
]);

})();
