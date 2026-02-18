// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --turbofan
// Flags: --typed-array-length-loading

function foo(size) {
  let a = new Uint8Array(size);
  let b = new Uint8Array(size + 1);
  b[a.length] = 1;
  return b;
}
%PrepareFunctionForOptimization(foo);

foo(100);

%OptimizeFunctionOnNextCall(foo);
const a1 = foo(100);
assertEquals(1, a1[100]);
assertOptimized(foo);

// TODO(389019544): This might or might not fail once the deopt loop is fixed.
if (%Is64Bit()) {
  try {
    const largeLength = 8589934592;
    const a2 = foo(largeLength);
    assertEquals(1, a2[largeLength]);
    assertUnoptimized(foo);
  } catch (e) {
    // If alloating the TypedArray failed, we'll get a RangeError. Other
    // errors are just normal test failures.
    assertTrue(e instanceof RangeError);
  }
}
