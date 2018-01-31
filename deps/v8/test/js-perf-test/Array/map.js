// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function benchy(name, test, testSetup) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test, testSetup, ()=>{})
      ]);
}

benchy('NaiveMapReplacement', NaiveMap, NaiveMapSetup);
benchy('DoubleMap', DoubleMap, DoubleMapSetup);
benchy('SmallSmiToDoubleMap', SmiMap, SmiToDoubleMapSetup);
benchy('SmallSmiToFastMap', SmiMap, SmiToFastMapSetup);
benchy('SmiMap', SmiMap, SmiMapSetup);
benchy('FastMap', FastMap, FastMapSetup);
benchy('GenericMap', GenericMap, ObjectMapSetup);
benchy('OptFastMap', OptFastMap, FastMapSetup);

var array;
// Initialize func variable to ensure the first test doesn't benefit from
// global object property tracking.
var func = 0;
var this_arg;
var result;
var array_size = 100;

// Although these functions have the same code, they are separated for
// clean IC feedback.
function DoubleMap() {
  result = array.map(func, this_arg);
}
function SmiMap() {
  result = array.map(func, this_arg);
}
function FastMap() {
  result = array.map(func, this_arg);
}

// Make sure we inline the callback, pick up all possible TurboFan
// optimizations.
function RunOptFastMap(multiple) {
  // Use of variable multiple in the callback function forces
  // context creation without escape analysis.
  //
  // Also, the arrow function requires inlining based on
  // SharedFunctionInfo.
  result = array.map((v, i, a) =>  v + ' ' + multiple);
}

// Don't optimize because I want to optimize RunOptFastMap with a parameter
// to be used in the callback.
%NeverOptimizeFunction(OptFastMap);
function OptFastMap() { RunOptFastMap(3); }

function NaiveMap() {
  let index = -1
  const length = array == null ? 0 : array.length
  const result = new Array(length)

  while (++index < length) {
    result[index] = func(array[index], index, array)
  }
  return result
}


function GenericMap() {
  result = Array.prototype.map.call(array, func, this_arg);
}

function NaiveMapSetup() {
  // Prime NaiveMap with polymorphic cases.
  array = [1, 2, 3];
  func = (v, i, a) => v;
  NaiveMap();
  NaiveMap();
  array = [3.4]; NaiveMap();
  array = new Array(10); array[0] = 'hello'; NaiveMap();
  SmiMapSetup();
  delete array[1];
}

function SmiMapSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = i;
  func = (value, index, object) => { return value; };
}

function SmiToDoubleMapSetup() {
  array = new Array();
  for (var i = 0; i < 1; i++) array[i] = i;
  func = (value, index, object) => { return value + 0.5; };
}

function SmiToFastMapSetup() {
  array = new Array();
  for (var i = 0; i < 1; i++) array[i] = i;
  func = (value, index, object) => { return "hi" + value; };
}

function DoubleMapSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = (i + 0.5);
  func = (value, index, object) => { return value; };
}

function FastMapSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = 'value ' + i;
  func = (value, index, object) => { return value; };
}

function ObjectMapSetup() {
  array = { length: array_size };
  for (var i = 0; i < array_size; i++) {
    array[i] = i;
  }
  func = (value, index, object) => { return value; };
}
