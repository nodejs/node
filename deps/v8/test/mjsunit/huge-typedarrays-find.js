// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging

d8.file.execute('test/mjsunit/typedarray-helpers.js');
d8.file.execute('test/mjsunit/huge-typedarrays-helpers.js');

// Test `find`, `findIndex`, `findLast`, and `findLastIndex` methods.
for (let test of GetTestConfigs()) {
  let {arr, num_elems} = test;

  arr[13] = 11;
  arr[num_elems - 13] = 15;
  assertEquals(11, arr.find(e => e != 0));
  assertEquals(13, arr.findIndex(e => e != 0));
  assertEquals(15, arr.findLast(e => e != 0));
  assertEquals(num_elems - 13, arr.findLastIndex(e => e != 0));

  // Check that the callback arguments are as expected.
  let last_checked_index = num_elems;
  let callback = (elem, idx, array) => {
    assertSame(array, arr);
    assertEquals(last_checked_index - 1, idx);
    last_checked_index = idx;
    assertEquals(idx == num_elems - 13 ? 15 : 0, elem);
    return elem != 0;
  };
  assertEquals(15, arr.findLast(callback));
  assertEquals(num_elems - 13, last_checked_index);

  last_checked_index = num_elems;
  assertEquals(num_elems - 13, arr.findLastIndex(callback));
  assertEquals(num_elems - 13, last_checked_index);
}
