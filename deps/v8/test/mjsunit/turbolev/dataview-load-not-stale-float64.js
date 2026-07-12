// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev --clear-free-memory

// This test checks that repeated loads from the same DataView work correctly:
// the DataView should not be freed by GCs in between the loads.

function foo(x) {
  let dv = new DataView(new ArrayBuffer(16));

  // We need to be reading a value that's not 0 because --clear-free-memory sets
  // freed memory to 0 and thus we won't be able to tell if we're reading stale
  // memory or not.
  dv.setFloat64(0, 4.5);
  dv.setFloat64(8, 1.195);

  // Loading a first value from the DataView. This will compute the data pointer
  // that can be reused for subsequent loads.
  let v1 = dv.getFloat64(8);

  // Triggering a GC.
  %MajorGCForCompilerTesting();

  // If we don't have anything retaining {dv} and we GVNed the computation of
  // the data pointer, then it will have been freed by the above. In that case,
  // we're about to access a stale pointer.
  let v2 = dv.getFloat64(0);

  // Returning v1+v2 so that they don't get DCEed.
  return v1 + v2;
}

%PrepareFunctionForOptimization(foo);
assertEquals(5.695, foo());
assertEquals(5.695, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(5.695, foo());
