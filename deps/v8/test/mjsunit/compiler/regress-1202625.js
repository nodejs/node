// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  var __v_10 = { x: 1 };
  var __v_11 = { y: 1 };
  for (var i = 0; i < 1; i = {}) {
    i += Math.abs(__v_10.x);
    i += Math.abs(__v_10.x);
    __v_10 = __v_11;
  }
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
