// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function g(b) {
  const _ = Object.getOwnPropertyDescriptors(g);
  // for (const _ of b) {}
}

function f(...a) {
  g(a);
}

%PrepareFunctionForOptimization(f);
%PrepareFunctionForOptimization(g);
f([]);
%OptimizeFunctionOnNextCall(f);
f([]);
