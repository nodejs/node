// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('Call', [1000], [
  new Benchmark('Call-Sum', false, false, 0,
                CallSum, CallSumSetup,
                CallSumTearDown),
]);

new BenchmarkSuite('CallMethod', [1000], [
  new Benchmark('CallMethod-Sum', false, false, 0,
                CallMethodSum, CallSumSetup, CallMethodSumTearDown),
]);

new BenchmarkSuite('CallNew', [1000], [
  new Benchmark('CallNew-Sum', false, false, 0,
                CallNewSum, CallSumSetup,
                CallNewSumTearDown),
]);

var result;
var objectToSpread;

function sum() {
  var result = arguments[0];
  for (var i = 1; i < arguments.length; ++i) {
    result += arguments[i];
  }
  return result;
}

function CallSumSetup() {
  result = undefined;
  objectToSpread = [];
  for (var i = 0; i < 100; ++i) objectToSpread.push(i + 1);
}

function CallSum() {
  result = sum(...objectToSpread);
}

function CallSumTearDown() {
  var expected = 100 * (100 + 1) / 2;
  return result === expected;
}

// ----------------------------------------------------------------------------

var O = { sum: sum };
function CallMethodSum() {
  result = O.sum(...objectToSpread);
}

function CallMethodSumTearDown() {
  var expected = 100 * (100 + 1) / 2;
  return result === expected;
}

// ----------------------------------------------------------------------------

function Sum() {
  var result = arguments[0];
  for (var i = 1; i < arguments.length; ++i) {
    result += arguments[i];
  }
  return this.sum = result;
}

function CallNewSum() {
  result = new Sum(...objectToSpread);
}

function CallNewSumTearDown() {
  var expected = 100 * (100 + 1) / 2;
  return result instanceof Sum && result.sum === expected;
}
