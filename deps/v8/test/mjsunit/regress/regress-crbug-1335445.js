// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test checks that indexOf correctly sees that -0.0 == 0 when using its
// SIMD fast path on unaligned FixedDoubleArrays. The issue with testing that is
// that when creating an array from JavaScript, it's not possible to ensure that
// it will be unaligned. Thus, we wrapped the test in a loop, which additionally
// allocates some objects (or variable size), so that the array is eventually
// not aligned. In practice, this looks fairly reliable: about half the arrays
// are aligned on 8 bytes, and half are not.

// We store all the objects we create in this array, so that escape analysis
// doesn't get rid of allocations.
let objects = [];

for (let i = 0; i < 100; i++) {

  for (let x = 0; x < i; x++) {
    if (i % 2 == 0) {
      objects.push({ i: 35 });
    } else {
      objects.push({ i: 35, "a":42 });
    }
  }

  let arr = Array();
  objects.push(arr);
  for (let i = 0; i < 100; i++) {
    arr[i] = 1.5;
  }

  arr[20] = -0.0;
  arr[23] = 0;

  assertEquals(20, arr.indexOf(0));
}
