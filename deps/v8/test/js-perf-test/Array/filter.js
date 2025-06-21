// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

// From the lodash implementation.
function NaiveFilter() {
  let index = -1
  let resIndex = 0
  const length = array == null ? 0 : array.length
  const result = []

  while (++index < length) {
    const value = array[index]
    if (func(value, index, array)) {
      result[resIndex++] = value
    }
  }
  return result
}

function NaiveFilterSetup() {
  // Prime NaiveFilter with polymorphic cases.
  array = [1, 2, 3];
  NaiveFilter();
  NaiveFilter();
  array = [3.4]; NaiveFilter();
  array = new Array(10); array[0] = 'hello'; NaiveFilter();
  SmiSetup();
  delete array[1];
}

// Make sure we inline the callback, pick up all possible TurboFan
// optimizations.
function RunOptFastFilter(multiple) {
  // Use of variable multiple in the callback function forces
  // context creation without escape analysis.
  //
  // Also, the arrow function requires inlining based on
  // SharedFunctionInfo.
  result = array.filter((v, i, a) => multiple === 3);
}

// Don't optimize because I want to optimize RunOptFastMap with a parameter
// to be used in the callback.
%NeverOptimizeFunction(OptFastFilter);
function OptFastFilter() { RunOptFastFilter(3); }

function side_effect(a) { return a; }
%NeverOptimizeFunction(side_effect);
function OptUnreliableFilter() {
  result = array.filter(func, side_effect(array));
}

DefineHigherOrderTests([
  // name, test function, setup function, user callback
  ['NaiveFilterReplacement', NaiveFilter, NaiveFilterSetup, v => true],
  [
    'DoubleFilter', newClosure('filter'), DoubleSetup,
    v => Math.floor(v) % 2 === 0
  ],
  ['SmiFilter', newClosure('filter'), SmiSetup, v => v % 2 === 0],
  ['FastFilter', newClosure('filter'), FastSetup, (_, i) => i % 2 === 0],
  [
    'GenericFilter', newClosure('filter', true), ObjectSetup,
    (_, i) => i % 2 === 0
  ],
  ['OptFastFilter', OptFastFilter, FastSetup, undefined],
  ['OptUnreliableFilter', OptUnreliableFilter, FastSetup, v => true]
]);

})();
