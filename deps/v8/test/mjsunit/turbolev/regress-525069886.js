// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future

function f(x) {
  let i = 0;
  if (x) {
  } else {
    if (x === 0) {
    }
  }
  while (i < 5) {
    i++;
  }
}
%PrepareFunctionForOptimization(f);
f(1);
%OptimizeFunctionOnNextCall(f);
f();
