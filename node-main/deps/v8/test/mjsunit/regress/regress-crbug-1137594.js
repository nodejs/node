// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

v = { symbol: Symbol() };
function f() {
  for (var i = 0; i < 1; ++i) {
    try { v.symbol(); } catch (e) {}
  }
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
