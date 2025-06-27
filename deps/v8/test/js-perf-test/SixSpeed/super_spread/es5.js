// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This benchmark is based on the six-speed benchmark build output.
// Copyright 2014 Kevin Decker <https://github.com/kpdecker/six-speed/>

new BenchmarkSuite('ES5', [1000], [
  new Benchmark('ES5', false, false, 0, ES5),
]);

function Point(x, y) {
  this.x = x;
  this.y = y;
}

function MyPoint() {
  Point.apply(this, arguments);
}

function makePoint(x, y) {
  return new MyPoint(x, y);
}

function ES5() {
  'use strict';
  return makePoint(1, 2);
}
