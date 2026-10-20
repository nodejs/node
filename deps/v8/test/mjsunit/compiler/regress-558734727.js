// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax

function f1(a, b) { return a + b + 1; }
function f2(a, b) { return a + b + 2; }

function test(cond) {
  var f = cond ? f1 : f2;
  var v0 = f;
  var v1 = 1, v2 = 2, v3 = 3, v4 = 4, v5 = 5, v6 = 6, v7 = 7;
  var v8 = (v1 ? f : f);
  var v9 = 9, v10 = 10, v11 = 11, v12 = 12, v13 = 13, v14 = 14, v15 = 15;
  var res = f(v1, v15);
  var temp0 = v0;
  temp0 = 0;
  var temp8 = v8;
  return res + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v9 + v10 + v11 + v12 + v13 + v14 + v15;
}

%PrepareFunctionForOptimization(f1);
%PrepareFunctionForOptimization(f2);
%PrepareFunctionForOptimization(test);
assertEquals(129, test(true));
assertEquals(130, test(false));
%OptimizeFunctionOnNextCall(test);
assertEquals(129, test(true));
assertEquals(130, test(false));
