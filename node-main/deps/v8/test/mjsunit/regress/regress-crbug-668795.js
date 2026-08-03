// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function g() {
  return g.arguments;
}

function f() {
  var result = "R:";
  for (var i = 0; i < 3; ++i) {
    if (i == 1) %OptimizeOsr();
    result += g([1])[0];
    result += g([2])[0];
  }
  return result;
}
%PrepareFunctionForOptimization(f);

assertEquals("R:121212", f());
