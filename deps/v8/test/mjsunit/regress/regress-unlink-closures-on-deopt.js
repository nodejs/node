// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --noalways-turbofan

function foo() {
  function g(o) {
    return o.f;
  }
  return g;
}

let g1 = foo();
let g2 = foo();
%PrepareFunctionForOptimization(g1);

g1({ f : 1});
g1({ f : 2});
g2({ f : 2});
g2({ f : 2});

%OptimizeFunctionOnNextCall(g1);
g1({ f : 1});

%PrepareFunctionForOptimization(g2);
%OptimizeFunctionOnNextCall(g2);
g2({ f : 2});
g1({});

assertUnoptimized(g1);

// Deoptimization of g1 should also deoptimize g2 because they should share
// the optimized code object.  (Unfortunately, this test bakes in various
// other assumptions about dealing with optimized code, and thus might break
// in future.)
assertUnoptimized(g2);

g2({});
