// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging

d8.file.execute('test/mjsunit/typedarray-helpers.js');
d8.file.execute('test/mjsunit/huge-typedarrays-helpers.js');

// Test the `subarray` method.
for (let test of GetTestConfigs()) {
  let {arr, num_elems} = test;

  arr[13] = 11;
  arr[num_elems - 3] = 15;

  let sub = (begin, end) => Array.from(arr.subarray(begin, end));
  assertEquals([0, 11, 0], sub(12, 15));
  assertEquals([0, 15, 0], sub(num_elems - 4, num_elems - 1));
  assertEquals([0, 15, 0, 0], sub(num_elems - 4));
}
