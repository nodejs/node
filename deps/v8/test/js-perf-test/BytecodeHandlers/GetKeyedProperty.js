// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addBenchmark(name, test) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test)
      ]);
}

function objectLookupStringConstant() {
  const x = { 1: "foo" };

  for (var i = 0; i < 1000; ++i) {
    const p = x["1"];
  }
}

function objectLookupIndexNumber() {
  const x = { 1: "foo" };
  const a = 1;

  for (var i = 0; i < 1000; ++i) {
    const p = x[a];
  }
}

function objectLookupIndexString() {
  const x = { 1: "foo" };
  const a = "1";

  for (var i = 0; i < 1000; ++i) {
    const p = x[a];
  }
}

addBenchmark('Object-Lookup-String-Constant', objectLookupStringConstant);
addBenchmark('Object-Lookup-Index-Number', objectLookupIndexNumber);
addBenchmark('Object-Lookup-Index-String', objectLookupIndexString);
