// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function g(x, y) {
  return x | y;
}

function f(b) {
  if (b) {
    var s = g("a", "b") && true;
    return s;
  }
}

// Prime function g with Smi feedback.
g(1, 2);
g(1, 2);

f(0);
f(0);
%OptimizeFunctionOnNextCall(f);
// Compile inlined function g with string inputs but Smi feedback.
f(1);
