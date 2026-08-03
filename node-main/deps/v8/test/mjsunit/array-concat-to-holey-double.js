// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function ArrayConcat() {
  var a = [0];
  a[2] = 2;

  var b = [3.5,,4.5];

  const r = a.concat(b);

  assertEquals(0, r[0]);
  assertTrue(!Object.hasOwn(r, 1));
  assertEquals(2, r[2]);
  assertEquals(3.5, r[3]);
  assertTrue(!Object.hasOwn(r, 4));
  assertEquals(4.5, r[5]);
  assertTrue(%HasDoubleElements(r));
})();

(function ArrayConcatSlowPath() {
  var a = [0, 1, 2];
  a.foo = "hello world";

  var b = Array(4096).fill(3.5);
  b[4098] = 4.5;

  const r = a.concat(b);

  assertEquals(0, r[0]);
  assertEquals(1, r[1]);
  assertEquals(2, r[2]);
  assertEquals(3.5, r[3]);
  assertEquals(3.5, r[4098]);
  assertTrue(!Object.hasOwn(r, 4099));
  assertTrue(!Object.hasOwn(r, 4100));
  assertEquals(4.5, r[4101]);
  assertTrue(%HasDoubleElements(r));
})();
