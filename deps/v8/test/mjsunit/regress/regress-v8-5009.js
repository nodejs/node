// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function fn1() {
}

function fn2() {
}

function fn3() {
}

function create(id) {
  // Just some `FunctionTemplate` to hang on
  var o = new version();

  o.id = id;
  o[0] = null;

  return o;
}

function setM1(o) {
  o.m1 = fn1;
}

function setM2(o) {
  o.m2 = fn2;
}

function setAltM2(o) {
  // Failing StoreIC happens here
  o.m2 = fn3;
}

function setAltM1(o) {
  o.m1 = null;
}

function test(o) {
  o.m2();
  o.m1();
}

var p0 = create(0);
var p1 = create(1);
var p2 = create(2);

setM1(p0);
setM1(p1);
setM1(p2);

setM2(p0);
setAltM2(p0);
setAltM1(p0);

setAltM2(p1);

setAltM2(p2);
test(p2);
