// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function benchy(name, test, testSetup) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test, testSetup, ()=>{})
      ]);
}

benchy('DoubleReduceRight', DoubleReduceRight, DoubleReduceRightSetup);
benchy('SmiReduceRight', SmiReduceRight, SmiReduceRightSetup);
benchy('FastReduceRight', FastReduceRight, FastReduceRightSetup);
benchy('OptFastReduceRight', OptFastReduceRight, FastReduceRightSetup);

var array;
// Initialize func variable to ensure the first test doesn't benefit from
// global object property tracking.
var func = 0;
var this_arg;
var result;
var array_size = 100;

// Although these functions have the same code, they are separated for
// clean IC feedback.
function DoubleReduceRight() {
  result = array.reduceRight(func, this_arg);
}
function SmiReduceRight() {
  result = array.reduceRight(func, this_arg);
}
function FastReduceRight() {
  result = array.reduceRight(func, this_arg);
}

// Make sure we inline the callback, pick up all possible TurboFan
// optimizations.
function RunOptFastReduceRight(multiple) {
  // Use of variable multiple in the callback function forces
  // context creation without escape analysis.
  //
  // Also, the arrow function requires inlining based on
  // SharedFunctionInfo.
  result = array.reduceRight((p, v, i, a) => p + multiple);
}

// Don't optimize because I want to optimize RunOptFastMap with a parameter
// to be used in the callback.
%NeverOptimizeFunction(OptFastReduceRight);
function OptFastReduceRight() { RunOptFastReduceRight(3); }

function SmiReduceRightSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = i;
  func = (prev, value, index, object) => { return prev + 1; };
}

function DoubleReduceRightSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = (i + 0.5);
  func = (prev, value, index, object) => { return prev + value; };
}

function FastReduceRightSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = 'value ' + i;
  func = (prev, value, index, object) => { return prev + value; };
}
