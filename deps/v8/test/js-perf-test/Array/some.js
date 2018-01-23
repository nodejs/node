// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

// Make sure we inline the callback, pick up all possible TurboFan
// optimizations.
function RunOptFastSome(multiple) {
  // Use of variable multiple in the callback function forces
  // context creation without escape analysis.
  //
  // Also, the arrow function requires inlining based on
  // SharedFunctionInfo.
  result = array.some((v, i, a) => multiple !== 3);
}

// Don't optimize because I want to optimize RunOptFastMap with a parameter
// to be used in the callback.
%NeverOptimizeFunction(OptFastSome);
function OptFastSome() { RunOptFastSome(3); }

function side_effect(a) { return a; }
%NeverOptimizeFunction(side_effect);
function OptUnreliableSome() {
  result = array.some(func, side_effect(array));
}

DefineHigherOrderTests([
  // name, test function, setup function, user callback
  "DoubleSome", mc("some"), DoubleSetup, v => v < 0.0,
  "SmiSome", mc("some"), SmiSetup, v => v === 34343,
  "FastSome", mc("some"), FastSetup, v => v === 'hi',
  "OptFastSome", OptFastSome, FastSetup, undefined,
  "OptUnreliableSome", OptUnreliableSome, FastSetup, v => v === 'hi'
]);

})();
