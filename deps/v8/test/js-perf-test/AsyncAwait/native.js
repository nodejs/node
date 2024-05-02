// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


new BenchmarkSuite('Native', [1000], [
  new Benchmark('Basic', false, false, 0, Basic, Setup),
]);

var a,b,c,d,e,f,g,h,i,j,x;

function Setup() {
  x = Promise.resolve();

  j = async function j() { return x; };
  i = async function i() {
    await j();
    await j();
    await j();
    await j();
    await j();
    await j();
    await j();
    await j();
    await j();
    return j();
  };
  h = async function h() { return i(); };
  g = async function g() { return h(); };
  f = async function f() { return g(); };
  e = async function e() { return f(); };
  d = async function d() { return e(); };
  c = async function c() { return d(); };
  b = async function b() { return c(); };
  a = async function a() { return b(); };

  %PerformMicrotaskCheckpoint();
}

function Basic() {
  a();
  %PerformMicrotaskCheckpoint();
}
