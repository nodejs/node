// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

function NaiveMap() {
  let index = -1
  const length = array == null ? 0 : array.length
  const result = new Array(length)

  while (++index < length) {
    result[index] = func(array[index], index, array)
  }
  return result
}

function NaiveMapSetup() {
  // Prime NaiveMap with polymorphic cases.
  array = [1, 2, 3];
  NaiveMap();
  NaiveMap();
  array = [3.4]; NaiveMap();
  array = new Array(10); array[0] = 'hello'; NaiveMap();
  SmiSetup();
  delete array[1];
}

// Make sure we inline the callback, pick up all possible TurboFan
// optimizations.
function RunOptFastMap(value) {
  // Use of variable {value} in the callback function forces
  // context creation without escape analysis.
  //
  // Also, the arrow function requires inlining based on
  // SharedFunctionInfo.
  result = array.map((v, i, a) =>  v + value);
}

// Don't optimize because I want to optimize RunOptFastMap with a parameter
// to be used in the callback.
%NeverOptimizeFunction(OptFastMap);
function OptFastMap() { RunOptFastMap(" 3"); }

function side_effect(a) { return a; }
%NeverOptimizeFunction(side_effect);
function OptUnreliableMap() {
  result = array.map(func, side_effect(array));
}

DefineHigherOrderTests([
  // name, test function, setup function, user callback
  ['NaiveMapReplacement', NaiveMap, NaiveMapSetup, v => v],
  ['SmiMap', newClosure('map'), SmiSetup, v => v],
  ['DoubleMap', newClosure('map'), DoubleSetup, v => v],
  ['FastMap', newClosure('map'), FastSetup, v => v],
  ['SmallSmiToDoubleMap', newClosure('map'), SmiSetup, v => v + 0.5],
  ['SmallSmiToFastMap', newClosure('map'), SmiSetup, v => 'hi' + v],
  ['GenericMap', newClosure('map', true), ObjectSetup, v => v],
  ['OptFastMap', OptFastMap, FastSetup, undefined],
  ['OptUnreliableMap', OptUnreliableMap, FastSetup, v => v]
]);

})();
