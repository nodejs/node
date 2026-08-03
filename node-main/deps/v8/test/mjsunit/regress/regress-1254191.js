// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function f(a) {
  let x = -1n;
  if (!a) {
    x = a;
  }
  x|0;
}

%PrepareFunctionForOptimization(f);
f(false);
%OptimizeFunctionOnNextCall(f);
assertThrows(() => f(true), TypeError);
