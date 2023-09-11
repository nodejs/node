// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This benchmark is based on the six-speed benchmark build output.
// Copyright 2014 Kevin Decker <https://github.com/kpdecker/six-speed/>

new BenchmarkSuite(
  "ES6",
  [1000],
  [new Benchmark("ES6", false, false, 0, ES6, Setup)]
);

var map;

function Setup() {
  map = new Map();

  for (var i = 0; i < 500; i++) {
    map.set(i + "", i);
  }
}

function ES6() {
  return map.get("499") === 499;
}
