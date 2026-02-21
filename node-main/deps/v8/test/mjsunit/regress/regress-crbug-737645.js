// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

for (let i = 0; i < 100; i++) {
  // - length > 2 to trigger sorting.
  // - key > kRequiresSlowElementsLimit required to set the according bit on the
  //   dictionary elements store.
  let key = 1073741800 + i;
  var a = { length: 12, 1: 0xFA, [key]: 0xFB };
  %HeapObjectVerify(a);
  assertEquals(["1", ""+key, "length"], Object.keys(a));
  // Sort, everything > length is ignored.
  Array.prototype.sort.call(a);
  %HeapObjectVerify(a);
  assertEquals(["0", ""+key, "length"], Object.keys(a));
  // Sorting again to trigger bug caused by not setting requires_slow_elements
  Array.prototype.sort.call(a);
  %HeapObjectVerify(a);
  assertEquals(["0", ""+key, "length"], Object.keys(a));
}
