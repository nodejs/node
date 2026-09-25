// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Comparing different iterating schemes against spread initial literals.
// Benchmarks for large smi sets.

var keys = Array.from(Array(1e4).keys());
var set = new Set(keys);

// ----------------------------------------------------------------------------
// Benchmark: Spread
// ----------------------------------------------------------------------------

function Spread() {
  var newArr = [...set];
  // basic sanity check
  if (newArr.length != set.size) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: SpreadKeys
// ----------------------------------------------------------------------------

function SpreadKeys() {
  var newArr = [...set.keys()];
  // basic sanity check
  if (newArr.length != set.size) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: SpreadValues
// ----------------------------------------------------------------------------

function SpreadValues() {
  var newArr = [...set.values()];
  // basic sanity check
  if (newArr.length != set.size) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForOf
// ----------------------------------------------------------------------------

function ForOf() {
  var newArr = new Array(set.size);
  var i = 0;
  for (let x of set) {
    newArr[i] = x;
    i++;
  }
  if (newArr.length != set.size) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForOfKeys
// ----------------------------------------------------------------------------

function ForOfKeys() {
  var newArr = new Array(set.size);
  var i = 0;
  for (let x of set.keys()) {
    newArr[i] = x;
    i++;
  }
  if (newArr.length != set.size) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForOfValues
// ----------------------------------------------------------------------------

function ForOfValues() {
  var newArr = new Array(set.size);
  var i = 0;
  for (let kv of set.values()) {
    newArr[i] = kv;
    i++;
  }
  if (newArr.length != set.size) throw 666;
  return newArr;
}


// ----------------------------------------------------------------------------
// Setup and Run
// ----------------------------------------------------------------------------

d8.file.execute('../base.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-ArrayLiteralInitialSpreadLargeSmiSet(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult('Error: ' + name, error);
  success = false;
}

function CreateBenchmark(name, f) {
  new BenchmarkSuite(name, [1000], [ new Benchmark(name, false, false, 0, f) ]);
}

CreateBenchmark('ForOf', ForOf);
CreateBenchmark('ForOfKeys', ForOfKeys);
CreateBenchmark('ForOfValues', ForOfValues);
CreateBenchmark('Spread', Spread);
CreateBenchmark('SpreadKeys', SpreadKeys);
CreateBenchmark('SpreadValues', SpreadValues);


BenchmarkSuite.config.doWarmup = true;
BenchmarkSuite.config.doDeterministic = false;
BenchmarkSuite.RunSuites({NotifyResult: PrintResult, NotifyError: PrintError});
