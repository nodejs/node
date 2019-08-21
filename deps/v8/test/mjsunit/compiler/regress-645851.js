// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  var sum = 0;
  while (1) {
    for (var j = 0; j < 200; j -=  j) {
      sum = sum + 1;
      %OptimizeOsr();
      if (sum == 2) return;
      %PrepareFunctionForOptimization(f);
    }
  }
  return sum;
}

%PrepareFunctionForOptimization(f);
f();
