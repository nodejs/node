// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

{
  const arr = new Array(%LeakHole(), 1.5);
  assertEquals(2, arr.length);
  assertEquals(undefined, arr[0]);
  assertEquals(1.5, arr[1]);
  assertFalse(0 in arr);
  assertTrue(1 in arr);
}

{
  const arr = new Array(1.5, %LeakHole());
  assertEquals(2, arr.length);
  assertEquals(1.5, arr[0]);
  assertEquals(undefined, arr[1]);
  assertTrue(0 in arr);
  assertFalse(1 in arr);
}

{
  const arr = new Array(%LeakHole(), undefined, 42, 1.5);
  assertEquals(4, arr.length);
  assertFalse(0 in arr);
  assertTrue(1 in arr);
  assertEquals(undefined, arr[1]);
  assertEquals(42, arr[2]);
  assertEquals(1.5, arr[3]);
}
