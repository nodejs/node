// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function p(x) {
  this.x = x;
}

function f() {
  var a = new p(1), b = new p(2);
  for (var i = 0; i < 1; i++) {
    a.x += b.x;
  }
  return a.x;
};
%PrepareFunctionForOptimization(f);
new p(0.1);  // make 'x' mutable box double field in p.

assertEquals(3, f());
assertEquals(3, f());
%OptimizeFunctionOnNextCall(f);
assertEquals(3, f());
