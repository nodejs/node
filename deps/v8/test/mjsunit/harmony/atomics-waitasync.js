// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function testOutOfBounds() {
  const sab = new SharedArrayBuffer(16);
  const i32a = new Int32Array(sab);
  assertThrows(() => {
    Atomics.waitAsync(i32a, 20, 0, 1000);
  }, RangeError);
})();

(function testValueNotEquals() {
  const sab = new SharedArrayBuffer(16);
  const i32a = new Int32Array(sab);

  const result = Atomics.waitAsync(i32a, 0, 1, 1000);
  assertEquals(false, result.async);
  assertEquals("not-equal", result.value);
})();

(function testZeroTimeout() {
  const sab = new SharedArrayBuffer(16);
  const i32a = new Int32Array(sab);

  const result = Atomics.waitAsync(i32a, 0, 0, 0);
  assertEquals(false, result.async);
  assertEquals("timed-out", result.value);
})();
