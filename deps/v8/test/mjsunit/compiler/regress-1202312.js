// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  var __v_0 = { x: 1 };
  var __v_1 = { };
  var __v_2 = 0;
  for (var i = 0; i < 1; i = {}) {
    __v_0 += __v_0.x + 0.5;
    __v_2 += __v_2.x % 0.5;
    __v_2 += __v_2.x % 0.5;
    __v_2 += __v_0.x < 6;
    __v_2 += __v_0.x === 7;
    __v_2 = __v_1;
  }
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
