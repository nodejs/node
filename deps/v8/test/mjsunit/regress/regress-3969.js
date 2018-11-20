// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Inner() {
  this.property = "OK";
  this.o2 = 1;
}

function Outer(inner) {
  this.inner = inner;
}

var inner = new Inner();
var outer = new Outer(inner);

Outer.prototype.boom = function() {
  return this.inner.property;
}

assertEquals("OK", outer.boom());
assertEquals("OK", outer.boom());
%OptimizeFunctionOnNextCall(Outer.prototype.boom);
assertEquals("OK", outer.boom());

inner = undefined;
%SetAllocationTimeout(0 /*interval*/, 2 /*timeout*/);
// Call something that will do GC while holding a handle to outer's map.
// The key is that this lets inner's map die while keeping outer's map alive.
delete outer.inner;

outer = new Outer({field: 1.51, property: "OK"});

assertEquals("OK", outer.boom());
