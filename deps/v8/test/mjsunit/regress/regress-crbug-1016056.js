// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(args) {
  eval();
  return arguments[0];
}
%PrepareFunctionForOptimization(f);
function g() {
  return f(1);
}
%PrepareFunctionForOptimization(g);
assertEquals(1, g());
%OptimizeFunctionOnNextCall(g);
assertEquals(1, g());
