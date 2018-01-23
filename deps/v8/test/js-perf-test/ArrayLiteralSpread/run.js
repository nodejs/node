// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


const input = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];


// ----------------------------------------------------------------------------
// Benchmark: Babel
// ----------------------------------------------------------------------------

function _toConsumableArray(arr) {
  if (Array.isArray(arr)) {
    for (var i = 0, arr2 = Array(arr.length); i < arr.length; i++) {
      arr2[i] = arr[i];
    }
    return arr2;
  } else {
    return Array.from(arr);
  }
}

function Babel() {
  const result = [0].concat(_toConsumableArray(input));
  if (result.length != 11) throw 666;
}


// ----------------------------------------------------------------------------
// Benchmark: ForOfPush
// ----------------------------------------------------------------------------


function ForOfPush() {
  const result = [0];
  for (const x of input) {
    result.push(x);
  }
  if (result.length != 11) throw 666;
}


// ----------------------------------------------------------------------------
// Benchmark: ForOfSet
// ----------------------------------------------------------------------------


function ForOfSet() {
  const result = [0];
  for (const x of input) {
    result[result.length] = x;
  }
  if (result.length != 11) throw 666;
}


// ----------------------------------------------------------------------------
// Benchmark: Spread
// ----------------------------------------------------------------------------


function Spread() {
  const result = [0, ...input];
  if (result.length != 11) throw 666;
}


// ----------------------------------------------------------------------------
// Setup and Run
// ----------------------------------------------------------------------------

load('../base.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-ArrayLiteralSpread(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}

function CreateBenchmark(name, f) {
  new BenchmarkSuite(name, [100], [ new Benchmark(name, false, false, 0, f) ]);
}

CreateBenchmark('Babel', Babel);
CreateBenchmark('ForOfPush', ForOfPush);
CreateBenchmark('ForOfSet', ForOfSet);
CreateBenchmark('Spread', Spread);

BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;
BenchmarkSuite.RunSuites({NotifyResult: PrintResult, NotifyError: PrintError});
