// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function g(a, b) {
  a = +a;
  if (b) {
    a = undefined;
  }
  print(a);
  return +a;
}

g(0);
g(0);
%OptimizeFunctionOnNextCall(g);
assertTrue(Number.isNaN(g(0, true)));
