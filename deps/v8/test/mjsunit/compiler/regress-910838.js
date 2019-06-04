// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(b, s, x) {
  if (!b) {
    return (x ? b : s * undefined) ? 1 : 42;
  }
}

function g(b, x) {
  return f(b, 'abc', x);
}

f(false, 0, 0);
g(true, 0);
%OptimizeFunctionOnNextCall(g);
assertEquals(42, g(false, 0));
