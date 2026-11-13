// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --hole-fuzzing --turbofan
// NO HARNESS

// Storing %LeakHole() into a constant data field and constant-folding the load
// in TurboFan must abort in JSGraph::ConstantNoHole.
const obj = {x: %LeakHole()};
function foo() {
  return obj.x;
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
