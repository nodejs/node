// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-opt --opt


const v0 = [];
function f(b) {
  for (let v13 = 0; v13 <= 3; v13 = v13 + 2241165261) {
    for (let i = 0; i < 8; i++) {}
    const v23 = Math.max(v13, -0.0, -2523259642);
    const v24 = v0[v23];
  }
};
%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
