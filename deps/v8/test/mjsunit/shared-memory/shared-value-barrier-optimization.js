// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct --allow-natives-syntax

const Box = new SharedStructType(['payload']);
let a, b;
function f() {
  a = SharedArray(4000);
  b = new Box();
  // Assignment into shared objects have a barrier that ensure the RHS is in
  // shared space.
  //
  // RHS needs to be large enough to be in a HeapNumber. TF then allocates it
  // out of the non-shared old space during optimization. If TF incorrectly
  // compiles away the barrier, TF optimized code will create shared->local
  // edges.
  a[0] = 2000000000;
  b.payload = 2000000000;
}
%PrepareFunctionForOptimization(f);
for (let i = 0; i < 10; i++) f();
// Verify that TF optimized code does not incorrectly compile away the shared
// value barrier.
%OptimizeFunctionOnNextCall(f);
for (let i = 0; i < 10; i++) f();
// SharedGC will verify there are no shared->local edges.
%SharedGC();
