// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function* g(level = 0) {
  if (level > 20) return;
  for (let x of [1, 2]) {
    yield x;
    yield* g(level + 1);
  }
}

%PrepareFunctionForOptimization(g);
let gen = g();
gen.next();
gen.next();
%OptimizeFunctionOnNextCall(g);
gen.next();
