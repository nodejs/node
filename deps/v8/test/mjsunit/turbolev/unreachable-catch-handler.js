// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This function relies on Turboshaft's MachineOptimizationReducer doing more
// powerful peephole optimizations / strength reduction than Maglev, in order to
// have a catch handler that Maglev thinks is reachable but Turboshaft realize
// isn't reachable. In particular, we rely on those 2 optimizations on Float64:
//
//   1.  a * 2   ==>  a + a
//   2.  a / -1  ==>  -a
//
// We use Float64 for the operation to avoid the overflow checks in the Maglev
// graph (which would make MachineOptimizationReducer's pattern matching fail),
// but we do the comparison with int32 inputs because Turboshaft doesn't
// constant fold `a == a` if `a` is Float64.
// (note that none of this is very future proof: as soon as we add those
// optimizations to Maglev, then Maglev will realize that the catch block isn't
// reachable, but that's ok).
function bar(a, x) {
  a += 0.1;
  try {
    if (x) { throw "throwing"; }
    if (((a * 2) | 0) != ((a + a) | 0)) {
      throw "blah";
    }
    if (((a / -1) | 0) != (-a | 0)) {
      throw "bleh";
    }
  } catch (e) {
    return 17;
  }
  return 42;
}

// Making sure that the catch handler in bar can be reached (otherwise Maglev
// would just skip it and do a LazyDeoptOnThrow).
%PrepareFunctionForOptimization(bar);
assertEquals(17, bar(1.5, 1));

function foo(a) {
  // Passing `0` as `x` which will allow Maglev to eliminate `if (x)`, thus
  // making the catch block unreachable (but Maglev won't realize this).
  return bar(a, 0);
}

%PrepareFunctionForOptimization(foo);
assertEquals(42, foo(1.2));
assertEquals(42, foo(1.2));

%OptimizeFunctionOnNextCall(foo);
assertEquals(42, foo(1.2));
