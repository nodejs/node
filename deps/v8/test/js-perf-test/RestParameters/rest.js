// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('Basic1', [1000], [
  new Benchmark('BasicRest1', false, false, 0,
                BasicRest1, BasicRest1Setup, BasicRest1TearDown)
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
