// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging

d8.file.execute('test/mjsunit/typedarray-helpers.js');
d8.file.execute('test/mjsunit/huge-typedarrays-helpers.js');

// Test the `entries`, `keys`, and `values` methods.
for (let test of GetTestConfigs()) {
  let {arr, num_elems} = test;

  // We can only test small indexes, because advancing the iterator returned
  // by `entries()` to the larger indexes would take too long.
  arr[2] = 7;
  arr[4] = 9;
  for (let [iterator, expected] of [
           [arr.entries(), [[0, 0], [1, 0], [2, 7], [3, 0], [4, 9], [5, 0]]],
           [arr.keys(), [0, 1, 2, 3, 4, 5]],
           [arr.values(), [0, 0, 7, 0, 9, 0]],
  ]) {
    for (let value of iterator) {
      assertEquals(expected.shift(), value);
      if (expected.length == 0) break;
    }
  }
}
