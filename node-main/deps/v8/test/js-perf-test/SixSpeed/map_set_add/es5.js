// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This benchmark is based on the six-speed benchmark build output.
// Copyright 2014 Kevin Decker <https://github.com/kpdecker/six-speed/>

new BenchmarkSuite("ES5", [1000], [new Benchmark("ES5", false, false, 0, ES5)]);

function ES5() {
  var map = {}, set = [];

  for (var i = 0; i < 250; i++) {
    map[i] = i;
    set.push(i);
  }

  map.foo = "bar";
  set.push("bar");
  return "foo" in map && set.indexOf("bar") >= 0;
}
