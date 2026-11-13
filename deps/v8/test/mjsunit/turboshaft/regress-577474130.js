// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

function transition_backing_store() {
  let o = { a: 1, b: 2, c: 3, d: 4 };
  // Use a separate local alias (`o2`) so `o2.e = 25` gets its own monomorphic
  // feedback slot instead of sharing a polymorphic transition+store slot with
  // `o.e = 42`.
  let o2 = o;
  o.e = 42;
  %MajorGCForCompilerTesting();
  o2.e = 25;
  return o;
}

%PrepareFunctionForOptimization(transition_backing_store);
transition_backing_store();
transition_backing_store();
%OptimizeFunctionOnNextCall(transition_backing_store);
assertEquals(25, transition_backing_store().e);
