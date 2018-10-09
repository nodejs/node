// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Comparing different copy schemes against spread initial literals.
// Benchmarks for small holey arrays.

const smallHoleyArray = Array(100);

for (var i = 0; i < 10; i++) {
  smallHoleyArray[i] = i;
}
for (var i = 90; i < 99; i++) {
  smallHoleyArray[i] = i;
}

// ----------------------------------------------------------------------------
// Benchmark: Spread
// ----------------------------------------------------------------------------

function SpreadSmallHoley() {
  var newArr = [...smallHoleyArray];
  // basic sanity check
  if (newArr.length != smallHoleyArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForLength
// ----------------------------------------------------------------------------

function ForLengthSmallHoley() {
  var newArr = new Array(smallHoleyArray.length);
  for (let i = 0; i < smallHoleyArray.length; i++) {
    newArr[i] = smallHoleyArray[i];
  }
  // basic sanity check
  if (newArr.length != smallHoleyArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForLengthEmpty
// ----------------------------------------------------------------------------

function ForLengthEmptySmallHoley() {
  var newArr = [];
  for (let i = 0; i < smallHoleyArray.length; i++) {
    newArr[i] = smallHoleyArray[i];
  }
  // basic sanity check
  if (newArr.length != smallHoleyArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: Slice
// ----------------------------------------------------------------------------

function SliceSmallHoley() {
  var newArr = smallHoleyArray.slice();
  // basic sanity check
  if (newArr.length != smallHoleyArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: Slice0
// ----------------------------------------------------------------------------

function Slice0SmallHoley() {
  var newArr = smallHoleyArray.slice(0);
  // basic sanity check
  if (newArr.length != smallHoleyArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ConcatReceive
// ----------------------------------------------------------------------------

function ConcatReceiveSmallHoley() {
  var newArr = smallHoleyArray.concat();
  // basic sanity check
  if (newArr.length != smallHoleyArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ConcatArg
// ----------------------------------------------------------------------------

function ConcatArgSmallHoley() {
  var newArr = [].concat(smallHoleyArray);
  // basic sanity check
  if (newArr.length != smallHoleyArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: ForOfPush
// ----------------------------------------------------------------------------

function ForOfPushSmallHoley() {
  var newArr = [];
  for (let x of smallHoleyArray) {
    newArr.push(x)
  }
  // basic sanity check
  if (newArr.length != smallHoleyArray.length) throw 666;
  return newArr;
}

// ----------------------------------------------------------------------------
// Benchmark: MapId
// ----------------------------------------------------------------------------

function MapIdSmallHoley() {
  var newArr = smallHoleyArray.map(x => x);
  // basic sanity check
  if (newArr.length != smallHoleyArray.length) throw 666;
  return newArr;
}


// ----------------------------------------------------------------------------
// Setup and Run
// ----------------------------------------------------------------------------

load('../base.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-ArrayLiteralInitialSpreadSmallHoley(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult('Error: ' + name, error);
  success = false;
}

function CreateBenchmark(name, f) {
  new BenchmarkSuite(name, [1000], [ new Benchmark(name, false, false, 0, f) ]);
}

CreateBenchmark('Spread', SpreadSmallHoley);
CreateBenchmark('ForLength', ForLengthSmallHoley);
CreateBenchmark('ForLengthEmpty', ForLengthEmptySmallHoley);
CreateBenchmark('Slice', SliceSmallHoley);
CreateBenchmark('Slice0', Slice0SmallHoley);
CreateBenchmark('ConcatReceive', ConcatReceiveSmallHoley);
CreateBenchmark('ConcatArg', ConcatArgSmallHoley);
CreateBenchmark('ForOfPush', ForOfPushSmallHoley);
CreateBenchmark('MapId', MapIdSmallHoley);

BenchmarkSuite.config.doWarmup = true;
BenchmarkSuite.config.doDeterministic = false;
BenchmarkSuite.RunSuites({NotifyResult: PrintResult, NotifyError: PrintError});
