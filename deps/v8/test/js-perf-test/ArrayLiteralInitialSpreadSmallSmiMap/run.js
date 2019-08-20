// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Comparing different iterating schemes against spread initial literals.
// Benchmarks for small smi maps.

var keys = Array.from(Array(50).keys());
var keyValuePairs = keys.map((value) => [value, value + 1]);
var map = new Map(keyValuePairs);

// ----------------------------------------------------------------------------
// Benchmark: SpreadKeys
// ----------------------------------------------------------------------------

function SpreadKeys() {
  var newArr = [...map.keys()];
  // basic sanity check
  if (newArr.length != map.size) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: SpreadValues
// ----------------------------------------------------------------------------

function SpreadValues() {
  var newArr = [...map.values()];
  // basic sanity check
  if (newArr.length != map.size) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForOfKeys
// ----------------------------------------------------------------------------

function ForOfKeys() {
  var newArr = new Array(map.size);
  var i = 0;
  for (let x of map.keys()) {
    newArr[i] = x;
    i++;
  }
  if (newArr.length != map.size) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForOfValues
// ----------------------------------------------------------------------------

function ForOfValues() {
  var newArr = new Array(map.size);
  var i = 0;
  for (let x of map.values()) {
    newArr[i] = x;
    i++;
  }
  if (newArr.length != map.size) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Setup and Run
// ----------------------------------------------------------------------------

load('../base.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-ArrayLiteralInitialSpreadSmallSmiMap(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult('Error: ' + name, error);
  success = false;
}

function CreateBenchmark(name, f) {
  new BenchmarkSuite(name, [1000], [ new Benchmark(name, false, false, 0, f) ]);
}

CreateBenchmark('ForOfKeys', ForOfKeys);
CreateBenchmark('ForOfValues', ForOfValues);
CreateBenchmark('SpreadKeys', SpreadKeys);
CreateBenchmark('SpreadValues', SpreadValues);


BenchmarkSuite.config.doWarmup = true;
BenchmarkSuite.config.doDeterministic = false;
BenchmarkSuite.RunSuites({NotifyResult: PrintResult, NotifyError: PrintError});
