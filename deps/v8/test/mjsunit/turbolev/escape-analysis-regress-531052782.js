// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

function bar() {}
%NeverOptimizeFunction(bar);

// We'll create a graph with a store to a fixed double array, where the store
// will be out of bounds but we won't have realized it yet during escape
// analysis, and when calling FixedDoubleArray::OffsetOfElementAt, we'll have an
// overflow that will produce an offset of 0.
const overflow_target = 0x100000000;
const offset_times_8 = overflow_target - 8;
const offset = offset_times_8 / 8;

function foo() {
  let arr = [3.3, 4.5];
  %AssertEscapeAnalysisElided(arr);

  arr[offset] = 3.35;

  // Opaque call that will need a lazy frame state, which will capture {arr},
  // which will force resolving its map.
  bar();
}

%PrepareFunctionForOptimization(foo);
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
