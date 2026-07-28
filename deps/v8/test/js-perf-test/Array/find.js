// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

// Make sure we inline the callback, pick up all possible TurboFan
// optimizations.
function RunOptFast(value) {
  // Use of variable {value} in the callback function forces
  // context creation without escape analysis.
  //
  // Also, the arrow function requires inlining based on
  // SharedFunctionInfo.
  result = array.find((v, i, a) => v === value);
}

// Don't optimize because I want to optimize RunOptFast with a parameter
// to be used in the callback.
%NeverOptimizeFunction(OptFast);
function OptFast() { RunOptFast(max_index_value); }

function side_effect(a) { return a; }
%NeverOptimizeFunction(side_effect);
function OptUnreliable() {
  result = array.find(func, side_effect(array));
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
  ['NaiveFindReplacement', Naive, NaiveSetup, v => v === max_index],
  ['DoubleFind', newClosure('find'), DoubleSetup, v => v === max_index + 0.5],
  ['SmiFind', newClosure('find'), SmiSetup, v => v === max_index],
  ['FastFind', newClosure('find'), FastSetup, v => v === max_index_value],
  ['GenericFind', newClosure('find', true), ObjectSetup, v => v === max_index],
  ['OptFastFind', OptFast, FastSetup, undefined],
  ['OptUnreliableFind', OptUnreliable, FastSetup, v => v === max_index]
]);

})();
