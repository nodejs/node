// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(x) {
  if (x != "x") {
    var o = {a: (x + 1) * 1.5};
    %DeoptimizeNow();
    return o.a;
  }
}

f(1.5); f(2.5); f(NaN);

function g(x) {
  f(""+x);
}

g("x"); g("x");
%OptimizeFunctionOnNextCall(g);
g("x");
