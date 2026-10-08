// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging

d8.file.execute('test/mjsunit/typedarray-helpers.js');
d8.file.execute('test/mjsunit/huge-typedarrays-helpers.js');

// Test the `every` method.
for (let test of GetTestConfigs()) {
  let {arr} = test;

  // Note: We cannot test an `every` that succeeds, because that would just
  // take too long. We are aborting after the element at `kChangedIndex`.
  let kChangedIndex = 1113;
  let kChangedValue = 47;
  let last_checked_index = -1;
  arr[kChangedIndex] = kChangedValue;
  assertFalse(arr.every((element, index, array) => {
    assertSame(arr, array);
    assertEquals(last_checked_index + 1, index);
    last_checked_index = index;
    assertEquals(index == kChangedIndex ? kChangedValue : 0, element);
    return element == 0;
  }));
  assertEquals(kChangedIndex, last_checked_index);
}
