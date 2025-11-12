// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function foo(...args) {
  if (args.length != 4) throw 666;
}

const front = [1, 2];
const mid = 3;
const back = function*() { yield 4 };


// ----------------------------------------------------------------------------
// Benchmark: SpreadCall
// ----------------------------------------------------------------------------

function SpreadCall() {
  foo(...front, mid, ...back());
}


// ----------------------------------------------------------------------------
// Benchmark: SpreadCallSpreadLiteral
// ----------------------------------------------------------------------------

function SpreadCallSpreadLiteral() {
  foo(...[...front, mid, ...back()]);
}


// ----------------------------------------------------------------------------
// Benchmark: ApplySpreadLiteral
// ----------------------------------------------------------------------------

function ApplySpreadLiteral() {
  foo.apply(this, [...front, mid, ...back()]);
}


// ----------------------------------------------------------------------------
// Setup and Run
// ----------------------------------------------------------------------------

d8.file.execute('../base.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-SpreadCallsGeneral(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}

function CreateBenchmark(name, f) {
  new BenchmarkSuite(name, [100], [ new Benchmark(name, false, false, 0, f) ]);
}

CreateBenchmark('ApplySpreadLiteral', ApplySpreadLiteral);
CreateBenchmark('SpreadCall', SpreadCall);
CreateBenchmark('SpreadCallSpreadLiteral', SpreadCallSpreadLiteral);

BenchmarkSuite.config.doWarmup = true;
BenchmarkSuite.config.doDeterministic = undefined;
BenchmarkSuite.RunSuites({NotifyResult: PrintResult, NotifyError: PrintError});
