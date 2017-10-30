// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This benchmark is based on the six-speed benchmark build output.
// Copyright 2014 Kevin Decker <https://github.com/kpdecker/six-speed/>

new BenchmarkSuite('ES6', [1000], [
  new Benchmark('ES6', false, false, 0, ES6),
]);

function ES6() {
  "use strict";
  return Math.max(...[1, 2, 3]);
}
