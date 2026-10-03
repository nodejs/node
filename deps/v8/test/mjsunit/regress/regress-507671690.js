// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

function f(x) {
  try {
    let acc = 0;
    const inner = elem => {
      acc += elem + x;
    };
    [4294967295].forEach(inner);
  } catch (e) {}
}

%PrepareFunctionForOptimization(f);
f(3);
f(3);
%OptimizeFunctionOnNextCall(f);
f();
