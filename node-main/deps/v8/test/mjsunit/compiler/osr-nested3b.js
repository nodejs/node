// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --use-osr

function f() {
  var sum = 0;
  for (var m = 99; m < 100; m++) {
    for (var i = 5; i < 6; i++) {
      for (var j = 0; j < 1000; j++) {
        var x = i + 2;
        var y = x + 5;
        var z = y + 3;
        sum += z;
        if (i == 25) %OptimizeOsr();
      }
      if (true) break;
    }
    if (true) break;
  }
  return sum;
}


assertEquals(15000, f());
assertEquals(15000, f());
assertEquals(15000, f());
