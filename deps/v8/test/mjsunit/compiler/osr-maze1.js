// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --use-osr

function bar(goal) {
  var count = 0;
  var sum = 11;
  var i = 35;
  %PrepareFunctionForOptimization(bar);
  while (i-- > 33) {
    if (count++ == goal) %OptimizeOsr();
    sum = sum + i;
  }
  %PrepareFunctionForOptimization(bar);
  while (i-- > 31) {
    if (count++ == goal) %OptimizeOsr();
    j = 9;
    %PrepareFunctionForOptimization(bar);
    while (j-- > 7) {
      if (count++ == goal) %OptimizeOsr();
      sum = sum + j * 3;
    }
    %PrepareFunctionForOptimization(bar);
    while (j-- > 5) {
      if (count++ == goal) %OptimizeOsr();
      sum = sum + j * 5;
    }
  }
  while (i-- > 29) {
    %PrepareFunctionForOptimization(bar);
    if (count++ == goal) %OptimizeOsr();
    while (j-- > 3) {
      var k = 10;
      %PrepareFunctionForOptimization(bar);
      if (count++ == goal) %OptimizeOsr();
      while (k-- > 8) {
        %PrepareFunctionForOptimization(bar);
        if (count++ == goal) %OptimizeOsr();
        sum = sum + k * 11;
      }
    }
    while (j-- > 1) {
      %PrepareFunctionForOptimization(bar);
      if (count++ == goal) %OptimizeOsr();
      while (k-- > 6) {
        %PrepareFunctionForOptimization(bar);
        if (count++ == goal) %OptimizeOsr();
        sum = sum + j * 13;
      }
    }
  }
  return sum;
}
%PrepareFunctionForOptimization(bar);

for (var i = 0; i < 13; i++) {
  %DeoptimizeFunction(bar);
  assertEquals(348, bar(i));
}
