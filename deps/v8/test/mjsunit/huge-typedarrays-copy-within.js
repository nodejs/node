// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging

d8.file.execute('test/mjsunit/typedarray-helpers.js');
d8.file.execute('test/mjsunit/huge-typedarrays-helpers.js');

// Test the `copyWithin` method.
for (let test of GetTestConfigs()) {
  let {arr, num_elems} = test;

  arr[2] = 2;
  arr.copyWithin(3, 1, 3);
  assertEquals([0, 2, 0, 2, 0], Array.from(arr.slice(1, 6)));
  arr.copyWithin(num_elems - 5, 1);
  assertEquals([0, 2, 0, 2, 0], Array.from(arr.slice(-5)));
}
