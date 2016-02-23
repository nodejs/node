// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function dummy(x) { };

function g() {
  return g.arguments;
}

function f(limit) {
  var i = 0;
  var o = {};
  for (; i < limit; i++) {
    o.y = +o.y;
    g();
  }
}

f(1);
f(1);
%OptimizeFunctionOnNextCall(f);
dummy(f(1));
dummy(f(2));
