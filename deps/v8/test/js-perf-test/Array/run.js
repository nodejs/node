// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('../base.js');

let array;
// Initialize func variable to ensure the first test doesn't benefit from
// global object property tracking.
let func = 0;
let this_arg;
let result;
const array_size = 100;
const max_index = array_size - 1;

// mc stands for "Make Closure," it's a handy function to get a fresh
// closure unpolluted by IC feedback for a 2nd-order array builtin
// test.
function mc(name, generic = false) {
  if (generic) {
    return new Function(
      `result = Array.prototype.${name}.call(array, func, this_arg);`);
  }
  return new Function(`result = array.${name}(func, this_arg);`);
}

function benchy(name, test, testSetup) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test, testSetup, ()=>{})
      ]);
}

function SmiSetup() {
  array = Array.from({ length: array_size }, (_, i) => i);
}

function DoubleSetup() {
  array = Array.from({ length: array_size }, (_, i) => i + 0.5);
}

function FastSetup() {
  array = Array.from({ length: array_size }, (_, i) => `value ${i}`);
}

function ObjectSetup() {
  array = { length: array_size };
  for (var i = 0; i < array_size; i++) {
    array[i] = i;
  }
}

function DefineHigherOrderTests(tests) {
  let i = 0;
  while (i < tests.length) {
     const name = tests[i++];
     const testFunc = tests[i++];
     const setupFunc = tests[i++];
     const callback = tests[i++];

     let setupFuncWrapper = () => {
       func = callback;
       this_arg = undefined;
       setupFunc();
     };
     benchy(name, testFunc, setupFuncWrapper);
  }
}

// Higher-order Array builtins.
load('filter.js');
load('map.js');
load('every.js');
load('some.js');
load('for-each.js');
load('reduce.js');
load('reduce-right.js');
load('find.js');
load('find-index.js');
load('of.js');

// Other Array builtins.
load('join.js');
load('to-string.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-Array(Score): ' + result);
}

function PrintStep(name) {}

function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}


BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError,
                           NotifyStep: PrintStep });
