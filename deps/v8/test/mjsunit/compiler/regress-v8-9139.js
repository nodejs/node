// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

let dummy = { x : {} };

let o = { x : 0.1 };

function f(o, a, b) {
  o.x = a + b;
}

%PrepareFunctionForOptimization(f);
f(o, 0.05, 0.05);
f(o, 0.05, 0.05);
%OptimizeFunctionOnNextCall(f);
f(o, 0.05, 0.05);
assertOptimized(f);
