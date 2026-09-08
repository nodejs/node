// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging

d8.file.execute('test/mjsunit/typedarray-helpers.js');
d8.file.execute('test/mjsunit/huge-typedarrays-helpers.js');

// Test operator[], at, set, slice.
for (let test of GetTestConfigs()) {
  let {arr, num_elems} = test;
  let slice = (start, end) => Array.from(arr.slice(start, end));

  for (let offset of [13, num_elems - 7]) {
    assertEquals(0, arr[offset]);
    assertEquals(0, arr.at(offset));
    arr[offset] = 11;
    assertEquals(0, arr[offset - 1]);
    assertEquals(0, arr.at(offset - 1));
    assertEquals(11, arr[offset]);
    assertEquals(11, arr.at(offset));
    assertEquals(0, arr[offset + 1]);
    assertEquals(0, arr.at(offset + 1));
    assertEquals([0, 11, 0], slice(offset - 1, offset + 2));
    arr.set([17, 21], offset + 1);
    assertEquals([0, 11, 17, 21, 0], slice(offset - 1, offset + 4));
  }
}
