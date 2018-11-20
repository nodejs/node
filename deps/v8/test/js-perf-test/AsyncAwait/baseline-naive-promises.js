// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


new BenchmarkSuite('BaselineNaivePromises', [1000], [
  new Benchmark('Basic', false, false, 0, Basic, Setup),
]);

var a,b,c,d,e,f,g,h,i,j,x;

function Setup() {
  x = Promise.resolve();

  j = function j(p) { return p; };
  i = function i(p) {
    return p.then(j)
            .then(j)
            .then(j)
            .then(j)
            .then(j)
            .then(j)
            .then(j)
            .then(j)
            .then(j)
            .then(j);
  };
  h = function h(p) { return p; };
  g = function g(p) { return p; };
  f = function f(p) { return p; };
  e = function e(p) { return p; };
  d = function d(p) { return p; };
  c = function c(p) { return p; };
  b = function b(p) { return p; };
  a = function a(p) { return p; };

  %RunMicrotasks();
}

function Basic() {
  x.then(j)
   .then(i)
   .then(h)
   .then(g)
   .then(f)
   .then(e)
   .then(d)
   .then(c)
   .then(b)
   .then(a);
  %RunMicrotasks();
}
