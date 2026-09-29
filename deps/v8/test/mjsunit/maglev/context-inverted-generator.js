// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* f() {
  yield function g() { return "" + test + (gen.next(), test) };
  var test = 10;
}

var gen = f();
var g = gen.next().value;
%PrepareFunctionForOptimization(g);
assertEquals("undefined10", g());

gen = f();
g = gen.next().value;
assertEquals("undefined10", g());

gen = f();
g = gen.next().value;
assertEquals("undefined10", g());

gen = f();
g = gen.next().value;
%OptimizeMaglevOnNextCall(g);
assertEquals("undefined10", g());
