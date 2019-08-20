// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test spreading of large holey arrays, which are supposedly allocated in
// LargeObjectSpace. Holes should be replaced with undefined.

var arr = new Array(2e5);

for (var i = 0; i < 10; i++) {
  arr[i] = i;
}

var arr2 = [...arr];
assertTrue(arr2.hasOwnProperty(10));
assertEquals(undefined, arr2[10]);
assertEquals(9, arr2[9]);
