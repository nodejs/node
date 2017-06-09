// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function Inner() {
  this.inner_name = "inner";
}

function Boom() {
  this.boom = "boom";
}

function Outer() {
  this.outer_name = "outer";
}

function SetInner(inner, value) {
  inner.prop = value;
}

function SetOuter(outer, value) {
  outer.inner = value;
}

var inner1 = new Inner();
var inner2 = new Inner();

SetInner(inner1, 10);
SetInner(inner2, 10);

var outer1 = new Outer();
var outer2 = new Outer();
var outer3 = new Outer();

SetOuter(outer1, inner1);
SetOuter(outer1, inner1);
SetOuter(outer1, inner1);

SetOuter(outer2, inner2);
SetOuter(outer2, inner2);
SetOuter(outer2, inner2);

SetOuter(outer3, inner2);
SetOuter(outer3, inner2);
SetOuter(outer3, inner2);


SetInner(inner2, 6.5);

outer1 = null;
inner1 = null;

gc();

var boom = new Boom();
SetOuter(outer2, boom);

gc();
