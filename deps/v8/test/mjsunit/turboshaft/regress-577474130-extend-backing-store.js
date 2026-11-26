// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

// Extending a JSObject's PropertyArray backing store must mark the store to
// `properties_or_hash_` as initializing/transitioning so StoreStoreElimination
// cannot drop earlier extensions across a GC.
function extend_backing_store() {
  let o = { a: 1, b: 2, c: 3, d: 4 };
  let o2 = o;
  let o3 = o;
  o.p1 = 1;
  o2.p2 = 2;
  o2.p3 = 3;
  o2.p4 = 4;
  o2.p5 = 5;
  o2.p6 = 6;
  %MajorGCForCompilerTesting();
  o3.p7 = 7;
  return o;
}

%PrepareFunctionForOptimization(extend_backing_store);
extend_backing_store();
extend_backing_store();
%OptimizeFunctionOnNextCall(extend_backing_store);
assertEquals(7, extend_backing_store().p7);
