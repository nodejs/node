// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Comparing different copy schemes against spread initial literals.
// Benchmarks for small packed double arrays.

var smallArray = Array.from(Array(100).keys());

for (var i = 0; i < 100; i++) {
  smallArray[i] += 6.66;
}


// ----------------------------------------------------------------------------
// Benchmark: Spread
// ----------------------------------------------------------------------------

function SpreadSmall() {
  var newArr = [...smallArray];
  // basic sanity check
  if (newArr.length != smallArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForLength
// ----------------------------------------------------------------------------

function ForLengthSmall() {
  var newArr = new Array(smallArray.length);
  for (let i = 0; i < smallArray.length; i++) {
    newArr[i] = smallArray[i];
  }
  // basic sanity check
  if (newArr.length != smallArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForLengthEmpty
// ----------------------------------------------------------------------------

function ForLengthEmptySmall() {
  var newArr = [];
  for (let i = 0; i < smallArray.length; i++) {
    newArr[i] = smallArray[i];
  }
  // basic sanity check
  if (newArr.length != smallArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: Slice
// ----------------------------------------------------------------------------

function SliceSmall() {
  var newArr = smallArray.slice();
  // basic sanity check
  if (newArr.length != smallArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: Slice0
// ----------------------------------------------------------------------------

function Slice0Small() {
  var newArr = smallArray.slice(0);
  // basic sanity check
  if (newArr.length != smallArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ConcatReceive
// ----------------------------------------------------------------------------

function ConcatReceiveSmall() {
  var newArr = smallArray.concat();
  // basic sanity check
  if (newArr.length != smallArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ConcatArg
// ----------------------------------------------------------------------------

function ConcatArgSmall() {
  var newArr = [].concat(smallArray);
  // basic sanity check
  if (newArr.length != smallArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForOfPush
// ----------------------------------------------------------------------------

function ForOfPushSmall() {
  var newArr = [];
  for (let x of smallArray) {
    newArr.push(x)
  }
  // basic sanity check
  if (newArr.length != smallArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: MapId
// ----------------------------------------------------------------------------

function MapIdSmall() {
  var newArr = smallArray.map(x => x);
  // basic sanity check
  if (newArr.length != smallArray.length) throw 666;
  return newArr;
}


// ----------------------------------------------------------------------------
// Setup and Run
// ----------------------------------------------------------------------------

load('../base.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-ArrayLiteralInitialSpreadSmallDoublePacked(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult('Error: ' + name, error);
  success = false;
}

function CreateBenchmark(name, f) {
  new BenchmarkSuite(name, [1000], [ new Benchmark(name, false, false, 0, f) ]);
}

CreateBenchmark('Spread', SpreadSmall);
CreateBenchmark('ForLength', ForLengthSmall);
CreateBenchmark('ForLengthEmpty', ForLengthEmptySmall);
CreateBenchmark('Slice', SliceSmall);
CreateBenchmark('Slice0', Slice0Small);
CreateBenchmark('ConcatReceive', ConcatReceiveSmall);
CreateBenchmark('ConcatArg', ConcatArgSmall);
CreateBenchmark('ForOfPush', ForOfPushSmall);
CreateBenchmark('MapId', MapIdSmall);


BenchmarkSuite.config.doWarmup = true;
BenchmarkSuite.config.doDeterministic = false;
BenchmarkSuite.RunSuites({NotifyResult: PrintResult, NotifyError: PrintError});
