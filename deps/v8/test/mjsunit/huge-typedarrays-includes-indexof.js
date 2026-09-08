// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging

d8.file.execute('test/mjsunit/typedarray-helpers.js');
d8.file.execute('test/mjsunit/huge-typedarrays-helpers.js');

// Test the `includes`, `indexOf`, and `lastIndexOf` methods.
for (let test of GetTestConfigs()) {
  let {arr, num_elems} = test;

  arr[13] = 11;
  arr[num_elems - 13] = 15;
  assertTrue(arr.includes(11));
  assertTrue(arr.includes(15, num_elems - 15));
  assertFalse(arr.includes(16, num_elems - 15));

  assertEquals(13, arr.indexOf(11));
  assertEquals(num_elems - 13, arr.indexOf(15, num_elems - 15));
  assertEquals(-1, arr.indexOf(16, num_elems - 15));

  assertEquals(13, arr.lastIndexOf(11, 25));
  assertEquals(num_elems - 13, arr.lastIndexOf(15));
  assertEquals(-1, arr.lastIndexOf(16, 25));
}
