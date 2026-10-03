// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan

function foo(array1, array2) {
  let len = array1.length;
  return array2[len];
}

if (%Is64Bit()) {
  const smallArray1 = new Uint8Array(8);
  const smallArray2 = new Uint8Array(10);
  const largeArray = new Uint8Array(2147483648); // 2^31

  %PrepareFunctionForOptimization(foo);
  foo(smallArray1, smallArray2);
  foo(smallArray1, smallArray2);

  %OptimizeFunctionOnNextCall(foo);
  foo(smallArray1, smallArray2);
  assertOptimized(foo);

  // Using largeArray's length (2^31) as the index for smallArray2
  // must trigger CheckedIntPtrToInt32 on the index and deoptimize foo.
  foo(largeArray, smallArray2);
  assertUnoptimized(foo);
}
