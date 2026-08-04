// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('Basic1', [1000], [
  new Benchmark('BasicRest1', false, false, 0,
                BasicRest1, BasicRest1Setup, BasicRest1TearDown)
]);

new BenchmarkSuite('ReturnArgsBabel', [10000], [
    new Benchmark('ReturnArgsBabel', false, false, 0,
                  ReturnArgsBabel, ReturnArgsBabelSetup,
                  ReturnArgsBabelTearDown)
]);

new BenchmarkSuite('ReturnArgsNative', [10000], [
    new Benchmark('ReturnArgsNative', false, false, 0,
                  ReturnArgsNative, ReturnArgsNativeSetup,
                  ReturnArgsNativeTearDown)
]);

// ----------------------------------------------------------------------------

var result;

function basic_rest_fn_1(factor, ...values) {
  var result = 0;
  for (var i = 0; i < values.length; ++i) {
    result += (factor * values[i]);
  }
  return result;
}

function BasicRest1Setup() {}

function BasicRest1() {
  result = basic_rest_fn_1(10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}

function BasicRest1TearDown() {
  return result == 550;
}

// ----------------------------------------------------------------------------

var length = 50;
var numbers = Array.apply(null, {length}).map(Number.call, Number);
var strings = numbers.map(String.call, String);

function ReturnArgsBabelFunction(unused) {
  "use strict";
  for (var _len = arguments.length, args = Array(_len > 1 ? _len - 1 : 0),
           _key = 1;
       _key < _len; _key++) {
    args[_key - 1] = arguments[_key];
  }
  return args;
}

function ReturnArgsBabelSetup() {
  // Warm up with HOLEY_ELEMENTS
  result = ReturnArgsBabelFunction(...strings);
  // Warm up with HOLEY_SMI_ELEMENTS
  result = ReturnArgsBabelFunction(...numbers);
}

function ReturnArgsBabel() {
  result = ReturnArgsBabelFunction(...strings);
  result = ReturnArgsBabelFunction(...numbers);
}

function ReturnArgsBabelTearDown() {
  return result.indexOf(0) === 0;
}

// ----------------------------------------------------------------------------

function ReturnArgsNativeFunction(unused, ...args) {
  return args;
}

function ReturnArgsNativeSetup() {
  // Warm up with HOLEY_ELEMENTS
  result = ReturnArgsNativeFunction(...strings);
  // Warm up with HOLEY_SMI_ELEMENTS
  result = ReturnArgsNativeFunction(...numbers);
}

function ReturnArgsNative() {
  result = ReturnArgsNativeFunction(...strings);
  result = ReturnArgsNativeFunction(...numbers);
}

function ReturnArgsNativeTearDown() {
  return result.indexOf(0) === 0;
}
