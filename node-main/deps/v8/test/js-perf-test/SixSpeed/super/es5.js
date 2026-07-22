// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This benchmark is based on the six-speed benchmark build output.
// Copyright 2014 Kevin Decker <https://github.com/kpdecker/six-speed/>

new BenchmarkSuite('ES5', [1000], [
  new Benchmark('ES5', false, false, 0, ES5),
]);

function C() {
  this.foo = 'bar';
}

C.prototype.bar = function() {
  return 41;
};

function D() {
  C.call(this);
  this.baz = 'bat';
}

D.prototype = Object.create(C.prototype);

D.prototype.bar = function() {
  return C.prototype.bar.call(this) + 1;
};

function ES5() {
  var d = new D();
  return d.bar();
}
