// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  this.x = 1;
}

var a = [];

// Create enough objects to trigger slack tracking.
for (let i = 0; i < 100; i++) {
  new f();
}

function h() {
  // Create a new object and add an out-of-object property 'y'.
  var o = new f();
  o.y = 1.5;
  return o;
}

function g(o) {
  // Add more properties so that we trigger extension of out-ot-object
  // property store.
  o.u = 1.1;
  o.v = 1.2;
  o.z = 1.3;
  // Return a field from the out-of-object-property store.
  return o.y;
}

g(h());
g(h());
%OptimizeFunctionOnNextCall(g);
assertEquals(1.5, g(h()));
