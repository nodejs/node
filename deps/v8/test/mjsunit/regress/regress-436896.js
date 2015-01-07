// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(x) {
  const x = 0;
  return x;
}

function g(x) {
  return f(x);
}

%OptimizeFunctionOnNextCall(g);
assertThrows(function() { g(42); }, TypeError);
