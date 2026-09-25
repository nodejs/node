// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupArray(length) {
  var a = new Array(length);
  for (var i=0;i<length;i++) {
    a[i] = ''+i;
  }
  return Object.freeze(a);
}

const frozenSpreadArray = setupArray(100);

function foo() {
  var result = arguments[0];
  for (var i = 1; i < arguments.length; ++i) {
    result += arguments[i];
  }
  return result;
}

// ----------------------------------------------------------------------------
// Benchmark: SpreadCall
// ----------------------------------------------------------------------------

function SpreadCall() {
  foo(...frozenSpreadArray);
}


// ----------------------------------------------------------------------------
// Benchmark: SpreadCallSpreadLiteral
// ----------------------------------------------------------------------------

function SpreadCallSpreadLiteral() {
  foo(...[...frozenSpreadArray]);
}


// ----------------------------------------------------------------------------
// Benchmark: ApplySpreadLiteral
// ----------------------------------------------------------------------------

function ApplySpreadLiteral() {
  foo.apply(this, [...frozenSpreadArray]);
}

// ----------------------------------------------------------------------------
// Setup and Run
// ----------------------------------------------------------------------------

function CreateBenchmark(name, f) {
  new BenchmarkSuite(name, [10], [ new Benchmark(name, false, false, 0, f) ]);
}

CreateBenchmark('ApplySpreadLiteral', ApplySpreadLiteral);
CreateBenchmark('SpreadCall', SpreadCall);
CreateBenchmark('SpreadCallSpreadLiteral', SpreadCallSpreadLiteral);
