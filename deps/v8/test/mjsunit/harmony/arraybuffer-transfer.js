// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab --allow-natives-syntax

assertEquals(0, ArrayBuffer.prototype.transfer.length);
assertEquals("transfer", ArrayBuffer.prototype.transfer.name);

function AssertDetached(ab) {
  assertEquals(0, ab.byteLength);
  assertThrows(() => (new Uint8Array(ab)).sort(), TypeError);
}

const IndicesAndValuesForTesting = [
  [1, 4],
  [4, 18],
  [17, 255],
  [518, 48]
];
function WriteTestData(ab) {
  let u8 = new Uint8Array(ab);
  for (let [idx, val] of IndicesAndValuesForTesting) {
    u8[idx] = val;
  }
}
function AssertBufferContainsTestData(ab) {
  let u8 = new Uint8Array(ab);
  for (let [idx, val] of IndicesAndValuesForTesting) {
    if (idx < u8.length) {
      assertEquals(val, u8[idx]);
    }
  }
}

function TestSameLength(len, opts) {
  let ab = new ArrayBuffer(len, opts);
  WriteTestData(ab);
  const xfer = ab.transfer();
  assertEquals(len, xfer.byteLength);
  assertFalse(xfer.resizable);
  AssertBufferContainsTestData(xfer);
  AssertDetached(ab);
}
TestSameLength(1024);
TestSameLength(1024, { maxByteLength: 2048 });

function TestGrow(len, opts) {
  let ab = new ArrayBuffer(len);
  WriteTestData(ab);
  const newLen = len * 2 + 128; // +128 to ensure newLen is never 0
  const xfer = ab.transfer(newLen);
  assertEquals(newLen, xfer.byteLength);
  assertFalse(xfer.resizable);
  if (len > 0) AssertBufferContainsTestData(xfer);
  AssertDetached(ab);

  // The new memory should be zeroed.
  let u8 = new Uint8Array(xfer);
  for (let i = len; i < newLen; i++) {
    assertEquals(0, u8[i]);
  }
}
TestGrow(1024);
TestGrow(1024, { maxByteLength: 2048 });
TestGrow(0);
TestGrow(0, { maxByteLength: 2048 });

function TestNonGrow(len, opts) {
  for (let newLen of [len / 2, // shrink
                      0        // 0 special case
                     ]) {
     let ab = new ArrayBuffer(len, opts);
    WriteTestData(ab);
    const xfer = ab.transfer(newLen);
    assertEquals(newLen, xfer.byteLength);
    assertFalse(xfer.resizable);
    if (len > 0) AssertBufferContainsTestData(xfer);
    AssertDetached(ab);
  }
}
TestNonGrow(1024);
TestNonGrow(1024, { maxByteLength: 2048 });
TestNonGrow(0);
TestNonGrow(0, { maxByteLength: 2048 });

(function TestParameterConversion() {
  const len = 1024;
  {
    let ab = new ArrayBuffer(len);
    const detach = { valueOf() { %ArrayBufferDetach(ab); return len; } };
    assertThrows(() => ab.transfer(detach), TypeError);
  }

  {
    let ab = new ArrayBuffer(len, { maxByteLength: len * 4 });
    const shrink = { valueOf() { ab.resize(len / 2); return len; } };
    const xfer = ab.transfer(shrink);
    assertFalse(xfer.resizable);
    assertEquals(len, xfer.byteLength);
  }

  {
    let ab = new ArrayBuffer(len, { maxByteLength: len * 4 });
    const grow = { valueOf() { ab.resize(len * 2); return len; } };
    const xfer = ab.transfer(grow);
    assertFalse(xfer.resizable);
    assertEquals(len, xfer.byteLength);
  }
})();

(function TestCannotBeSAB() {
  const len = 1024;
  let sab = new SharedArrayBuffer(1024);
  let gsab = new SharedArrayBuffer(len, { maxByteLength: len * 4 });

  assertThrows(() => ArrayBuffer.prototype.transfer.call(sab), TypeError);
  assertThrows(() => ArrayBuffer.prototype.transfer.call(gsab), TypeError);
})();

(function TestInvalidLength() {
  for (let newLen of [-1024, Number.MAX_SAFE_INTEGER + 1]) {
    let ab = new ArrayBuffer(1024);
    assertThrows(() => ab.transfer(newLen), RangeError);
  }
})();

(function TestEmptySourceStore() {
  let ab = new ArrayBuffer();
  let xfer = ab.transfer().transfer(1024);
})();

if (typeof WebAssembly !== 'undefined') {
  // WebAssembly buffers cannot be detached.
  const memory = new WebAssembly.Memory({ initial: 1 });
  assertThrows(() => memory.buffer.transfer(), TypeError);
}
