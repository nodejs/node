// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f1(n) {
  var a = n + 2;
  return a + 3;
}

%PrepareFunctionForOptimization(f1);
assertEquals(f1(1), 6);
assertEquals(f1(2), 7);
%OptimizeFunctionOnNextCall(f1);
assertEquals(f1(13), 18);
assertEquals(f1(14), 19);

function f2(n, odd) {
  var a = n + 2;
  if (odd) return a;
  return a + 3;
}

%PrepareFunctionForOptimization(f2);
assertEquals(f2(1, true), 3);
assertEquals(f2(2, false), 7);
%OptimizeFunctionOnNextCall(f2);
assertEquals(f2(13, true), 15);
assertEquals(f2(14, false), 19);
