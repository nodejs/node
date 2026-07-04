// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --mock-arraybuffer-allocator

function Asm(stdlib, foreign, buffer) {
  "use asm";

  var i32 = new stdlib.Int32Array(buffer);

  function get(index) {
    index = index|0;
    return i32[index >> 2] | 0;
  }

  return { get: get };
}

var heap;
var allocation_succeeded = false;
try {
  heap = new ArrayBuffer(0x8200_0000);
  allocation_succeeded = true;
} catch (e) {
  // On 32-bit platforms, the allocation will fail. Just skip the rest.
  assertTrue(e instanceof RangeError);
}

if (allocation_succeeded) {
  console.log("allocation successful, running actual test");
  var fast = Asm(this, null, heap);
  // This is an OOB access that should return 0 without trying to read from
  // the actual ArrayBuffer (which is inaccessible due to this test using
  // the mock allocator).
  // To make this test meaningful without the mock allocator, write a
  // recognizable value into the buffer, e.g.:
  //     let u8 = new Uint8Array(heap);
  //     u8[0x8000_0000] = 42;
  // The call below should then still return 0, not 42.
  assertEquals(0, fast.get(-0x8000_0000));
}
