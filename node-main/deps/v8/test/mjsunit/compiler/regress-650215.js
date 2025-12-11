// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  var x = 0;
  for (var i = 0; i < 10; i++) {
    x = (2 % x) | 0;
    if (i === 5) %OptimizeOsr();
  }
  return x;
}

%PrepareFunctionForOptimization(f);
assertEquals(0, f());
