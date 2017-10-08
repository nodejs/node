// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function benchy(name, test, testSetup) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test, testSetup, ()=>{})
      ]);
}

benchy('DoubleSome', DoubleSome, DoubleSomeSetup);
benchy('SmiSome', SmiSome, SmiSomeSetup);
benchy('FastSome', FastSome, FastSomeSetup);
benchy('OptFastSome', OptFastSome, FastSomeSetup);

var array;
// Initialize func variable to ensure the first test doesn't benefit from
// global object property tracking.
var func = 0;
var this_arg;
var result;
var array_size = 100;

// Although these functions have the same code, they are separated for
// clean IC feedback.
function DoubleSome() {
  result = array.some(func, this_arg);
}
function SmiSome() {
  result = array.some(func, this_arg);
}
function FastSome() {
  result = array.some(func, this_arg);
}

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

function SmiSomeSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = i;
  func = (value, index, object) => { return value === 34343; };
}

function DoubleSomeSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = (i + 0.5);
  func = (value, index, object) => { return value < 0.0; };
}

function FastSomeSetup() {
  array = new Array();
  for (var i = 0; i < array_size; i++) array[i] = 'value ' + i;
  func = (value, index, object) => { return value === 'hi'; };
}
