// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --use-osr --allow-natives-syntax

function f() {
  for (var k = 0; k < 2; k++) {
    for (var j = 0; j < 3; j++) {
      var sum = 0;
      for (var i = 0; i < 1000; i++) {
        if (i == 100) %OptimizeOsr();
        var x = i + 2;
        var y = x + 5;
        var z = y + 3;
        sum += z;
      }
      assertEquals(509500, sum);
      %PrepareFunctionForOptimization(f);
    }
    assertEquals(509500, sum);
  }

  assertEquals(509500, sum);
}
%PrepareFunctionForOptimization(f);
f();
