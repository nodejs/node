// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function g() {
  return 42;
}

var o = {};

function f(o, x) {
  o.f = x;
}

f(o, g);
f(o, g);
f(o, g);
assertEquals(42, o.f());
%OptimizeFunctionOnNextCall(f);
f(o, function() { return 0; });
assertEquals(0, o.f());
