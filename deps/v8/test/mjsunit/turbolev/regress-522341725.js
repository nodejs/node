// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

// Regression test for crbug.com/522341725. getAt is inlined into f so the
// Array.prototype.at reduction runs in the optimizer and builds a Select
// subgraph that ends in a deopt. Unlike crbug.com/522747101, the subgraph has
// buffered nodes that must still be flushed when it has no live exit.

function getAt() {
  return Array.prototype.at;
}
function f(a) {
  a[0];
  return getAt().call(a, -6);
}

%PrepareFunctionForOptimization(getAt);
%PrepareFunctionForOptimization(f);
f([]);
%OptimizeFunctionOnNextCall(f);
assertEquals(undefined, f([]));
