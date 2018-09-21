// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

// Make sure we inline the callback, pick up all possible TurboFan
// optimizations.
function RunOptFast(multiple) {
  // Use of variable multiple in the callback function forces
  // context creation without escape analysis.
  //
  // Also, the arrow function requires inlining based on
  // SharedFunctionInfo.
  result = array.findIndex((v, i, a) => v === `value ${multiple}`);
}

// Don't optimize because I want to optimize RunOptFast with a parameter
// to be used in the callback.
%NeverOptimizeFunction(OptFast);
function OptFast() { RunOptFast(max_index); }

function side_effect(a) { return a; }
%NeverOptimizeFunction(side_effect);
function OptUnreliable() {
  result = array.findIndex(func, side_effect(array));
}

function Naive() {
  let index = -1;
  const length = array == null ? 0 : array.length;

  for (let index = 0; index < length; index++) {
    const value = array[index];
    if (func(value, index, array)) {
      result = value;
      break;
    }
  }
}

function NaiveSetup() {
  // Prime Naive with polymorphic cases.
  array = [1, 2, 3];
  Naive();
  Naive();
  array = [3.4]; Naive();
  array = new Array(10); array[0] = 'hello'; Naive();
  SmiSetup();
  delete array[1];
}

DefineHigherOrderTests([
  // name, test function, setup function, user callback
  ['NaiveFindIndexReplacement', Naive, NaiveSetup, v => v === max_index],
  [
    'DoubleFindIndex', newClosure('findIndex'), DoubleSetup,
    v => v === max_index + 0.5
  ],
  ['SmiFindIndex', newClosure('findIndex'), SmiSetup, v => v === max_index],
  [
    'FastFindIndex', newClosure('findIndex'), FastSetup,
    v => v === `value ${max_index}`
  ],
  [
    'GenericFindIndex', newClosure('findIndex', true), ObjectSetup,
    v => v === max_index
  ],
  ['OptFastFindIndex', OptFast, FastSetup, undefined],
  ['OptUnreliableFindIndex', OptUnreliable, FastSetup, v => v === max_index]
]);

})();
