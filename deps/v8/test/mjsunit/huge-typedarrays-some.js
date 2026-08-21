// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging

d8.file.execute('test/mjsunit/typedarray-helpers.js');
d8.file.execute('test/mjsunit/huge-typedarrays-helpers.js');

// Test the `some` method.
for (let test of GetTestConfigs()) {
  let {arr} = test;

  // We need to set an element with a small index to avoid iterating for too
  // long.
  arr[13] = 11;
  let last_checked_index = -1;
  assertTrue(arr.some((elem, index, array) => {
    assertSame(array, arr);
    assertEquals(last_checked_index + 1, index);
    last_checked_index = index;
    assertEquals(index == 13 ? 11 : 0, elem);
    return elem != 0;
  }));
  assertEquals(13, last_checked_index);
}
