// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --use-osr --allow-natives-syntax

function f(x) {
  var sum = 0;
  var count = 10;
  while (count > 0) {
    sum += x;
    count--;
    if (count == 5) {
      %OptimizeOsr();
    }
  }
  return sum;
}
%PrepareFunctionForOptimization(f);

assertEquals(50, f(5));
