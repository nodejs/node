// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(x, len) {
  var distraction = [];
  var result = new Array(25);

  // Create a bunch of double values with long live ranges.
  var d0 = x + 0.5;
  var d1 = x + 1.5;
  var d2 = x + 2.5;
  var d3 = x + 3.5;
  var d4 = x + 4.5;
  var d5 = x + 5.5;
  var d6 = x + 6.5;
  var d7 = x + 7.5;
  var d8 = x + 8.5;
  var d9 = x + 9.5;
  var d10 = x + 10.5;
  var d11 = x + 11.5;
  var d12 = x + 12.5;
  var d13 = x + 13.5;
  var d14 = x + 14.5;
  var d15 = x + 15.5;
  var d16 = x + 16.5;
  var d17 = x + 17.5;
  var d18 = x + 18.5;
  var d19 = x + 19.5;
  var d20 = x + 20.5;
  var d21 = x + 21.5;
  var d22 = x + 22.5;
  var d23 = x + 23.5;
  var d24 = x + 24.5;

  // Trigger a stub failure when the array grows too big.
  distraction[len] = 0;

  // Write the long-lived doubles to memory and verify them.
  result[0] = d0;
  result[1] = d1;
  result[2] = d2;
  result[3] = d3;
  result[4] = d4;
  result[5] = d5;
  result[6] = d6;
  result[7] = d7;
  result[8] = d8;
  result[9] = d9;
  result[10] = d10;
  result[11] = d11;
  result[12] = d12;
  result[13] = d13;
  result[14] = d14;
  result[15] = d15;
  result[16] = d16;
  result[17] = d17;
  result[18] = d18;
  result[19] = d19;
  result[20] = d20;
  result[21] = d21;
  result[22] = d22;
  result[23] = d23;
  result[24] = d24;

  for (var i = 0; i < result.length; i++) {
    assertEquals(x + i + 0.5, result[i]);
  }
}

%PrepareFunctionForOptimization(f);
f(0, 10);
f(0, 10);
%OptimizeFunctionOnNextCall(f);
f(0, 80000);
