// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var o = {v:1};
var deopt = false;
Object.defineProperty(o, "x", {
  get: function() {
    if (deopt) %DeoptimizeFunction(foo);
    return 1;
  }
});

function bar(x, y, z) {
  return x + z;
}

function foo(o, x) {
  return bar(1, (o[x], 2), 3);
}

assertEquals(4, foo(o, "v"));
assertEquals(4, foo(o, "v"));
assertEquals(4, foo(o, "x"));
assertEquals(4, foo(o, "x"));
%OptimizeFunctionOnNextCall(foo);
deopt = true;
assertEquals(4, foo(o, "x"));
