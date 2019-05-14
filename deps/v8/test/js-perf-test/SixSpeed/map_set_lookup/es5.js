// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This benchmark is based on the six-speed benchmark build output.
// Copyright 2014 Kevin Decker <https://github.com/kpdecker/six-speed/>

new BenchmarkSuite(
  "ES5",
  [1000],
  [new Benchmark("ES5", false, false, 0, ES5, Setup)]
);

var keys, values, set, key;

function Setup() {
  (keys = []), (values = []), (set = []), (key = {});

  for (var i = 0; i < 500; i++) {
    keys.push(i);
    values.push(i);
    set.push(i);
  }

  keys.push(key);
  values.push("bar");
  set.push(key);
}

function ES5() {
  return set.indexOf(key) >= 0 && keys.indexOf(key) >= 0;
}
