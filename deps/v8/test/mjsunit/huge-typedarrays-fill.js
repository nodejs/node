// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging

d8.file.execute('test/mjsunit/typedarray-helpers.js');
d8.file.execute('test/mjsunit/huge-typedarrays-helpers.js');

// Test the `fill` method.
for (let test of GetTestConfigs()) {
  let {arr, num_elems} = test;

  arr.fill(13, num_elems - 3, num_elems - 1);
  assertEquals([0, 13, 13, 0], Array.from(arr.slice(-4)));
  arr.fill(17, num_elems - 2);
  assertEquals([0, 13, 17, 17], Array.from(arr.slice(-4)));
}
