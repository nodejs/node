// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Comparing different copy schemes against spread initial literals.
// Benchmarks for large packed arrays.

const largeHoleyArray = new Array(1e5);
const largeArray = Array.from(largeHoleyArray.keys());

// ----------------------------------------------------------------------------
// Benchmark: Spread
// ----------------------------------------------------------------------------

function SpreadLarge() {
  var newArr = [...largeArray];
  // basic sanity check
  if (newArr.length != largeArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForLength
// ----------------------------------------------------------------------------

function ForLengthLarge() {
  var newArr = new Array(largeArray.length);
  for (let i = 0; i < largeArray.length; i++) {
    newArr[i] = largeArray[i];
  }
  // basic sanity check
  if (newArr.length != largeArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForLengthEmpty
// ----------------------------------------------------------------------------

function ForLengthEmptyLarge() {
  var newArr = [];
  for (let i = 0; i < largeArray.length; i++) {
    newArr[i] = largeArray[i];
  }
  // basic sanity check
  if (newArr.length != largeArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: Slice
// ----------------------------------------------------------------------------

function SliceLarge() {
  var newArr = largeArray.slice();
  // basic sanity check
  if (newArr.length != largeArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: Slice0
// ----------------------------------------------------------------------------

function Slice0Large() {
  var newArr = largeArray.slice(0);
  // basic sanity check
  if (newArr.length != largeArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ConcatReceive
// ----------------------------------------------------------------------------

function ConcatReceiveLarge() {
  var newArr = largeArray.concat();
  // basic sanity check
  if (newArr.length != largeArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ConcatArg
// ----------------------------------------------------------------------------

function ConcatArgLarge() {
  var newArr = [].concat(largeArray);
  // basic sanity check
  if (newArr.length != largeArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForOfPush
// ----------------------------------------------------------------------------

function ForOfPushLarge() {
  var newArr = [];
  for (let x of largeArray) {
    newArr.push(x)
  }
  // basic sanity check
  if (newArr.length != largeArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: MapId
// ----------------------------------------------------------------------------

function MapIdLarge() {
  var newArr = largeArray.map(x => x);
  // basic sanity check
  if (newArr.length != largeArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Setup and Run
// ----------------------------------------------------------------------------

load('../base.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-ArrayLiteralInitialSpreadLargePacked(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult('Error: ' + name, error);
  success = false;
}

// Run the benchmark (5 x 100) iterations instead of 1 second.
function CreateBenchmark(name, f) {
  new BenchmarkSuite(name, [1000], [ new Benchmark(name, false, false, 5, f) ]);
}

CreateBenchmark('Spread', SpreadLarge);
CreateBenchmark('ForLength', ForLengthLarge);
CreateBenchmark('ForLengthEmpty', ForLengthEmptyLarge);
CreateBenchmark('Slice', SliceLarge);
CreateBenchmark('Slice0', Slice0Large);
CreateBenchmark('ConcatReceive', ConcatReceiveLarge);
CreateBenchmark('ConcatArg', ConcatArgLarge);
// The following benchmarks are so slow that they will time out.
// CreateBenchmark('ForOfPush', ForOfPushLarge);
// CreateBenchmark('MapId', MapIdLarge);

BenchmarkSuite.config.doWarmup = true;
BenchmarkSuite.config.doDeterministic = true;
BenchmarkSuite.RunSuites({NotifyResult: PrintResult, NotifyError: PrintError});
