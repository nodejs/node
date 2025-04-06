// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --typed-array-length-loading

const ab = new ArrayBuffer(100);
const rab = new ArrayBuffer(100, {maxByteLength: 200});

function foo(buffer) {
  const ta = (() => new Int32Array(buffer))();
  return ta.length;
}
%PrepareFunctionForOptimization(foo);

// Make the length access polymorphic with RAB/GSAB and non-RAB/GSAB
// TypedArrays.
assertEquals(25, foo(ab));
assertEquals(25, foo(rab));

%OptimizeFunctionOnNextCall(foo);

assertEquals(25, foo(ab));
assertEquals(25, foo(rab));
