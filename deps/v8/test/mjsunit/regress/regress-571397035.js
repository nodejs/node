// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const kBatchLength = 32;
const kInputLength = kBatchLength * 2;
const kTargetLength = kInputLength / 2;
const kDecodedByte = 0xaa;
const kInitialByte = 0x55;

function testSetFromHex(invalid_idx, is_two_byte, use_sab, invalid_pair) {
  const hex = kDecodedByte.toString(16).repeat(kTargetLength);
  assertTrue(invalid_idx < kInputLength);
  assertEquals(0, invalid_idx % 2);
  let str = hex.substring(0, invalid_idx) + invalid_pair +
      hex.substring(invalid_idx + invalid_pair.length);

  if (is_two_byte) {
    // Keep two-byte characters beyond the target's capacity.
    str += '\u0100\u0100';
  }
  str = %FlattenString(str);
  assertEquals(!is_two_byte, %StringIsOneByteRepresentation(str));
  assertEquals(0, str.length % 2);

  let target;
  if (use_sab) {
    target = new Uint8Array(new SharedArrayBuffer(kTargetLength));
  } else {
    target = new Uint8Array(kTargetLength);
  }

  target.fill(kInitialByte);

  assertThrows(() => target.setFromHex(str), SyntaxError);

  const valid_bytes = invalid_idx / 2;
  for (let i = 0; i < target.length; i++) {
    if (i < valid_bytes) {
      assertEquals(kDecodedByte, target[i], `Decoded byte at index ${i}`);
    } else {
      assertEquals(kInitialByte, target[i], `Untouched byte at index ${i}`);
    }
  }
}

// Invalid pair at char offsets 0, 2, 14, 16 and 30 of the first 32-char batch,
// and at the start of the second batch (offset 32).
const invalid_offsets = [0, 2, 14, 16, 30, kBatchLength];
const invalid_pairs = ['zz', 'za'];

for (let offset of invalid_offsets) {
  for (let is_two_byte of [false, true]) {
    for (let use_sab of [false, true]) {
      for (let pair of invalid_pairs) {
        testSetFromHex(offset, is_two_byte, use_sab, pair);
      }
    }
  }
}

// Odd-length input must still throw and write nothing.
function testOddLength(is_two_byte, use_sab) {
  let str = kDecodedByte.toString(16).repeat(kTargetLength / 2) + 'a';
  if (is_two_byte) {
    str += '\u0100\u0100';
  }
  str = %FlattenString(str);
  assertEquals(!is_two_byte, %StringIsOneByteRepresentation(str));
  assertEquals(1, str.length % 2);

  let target;
  if (use_sab) {
    target = new Uint8Array(new SharedArrayBuffer(kTargetLength));
  } else {
    target = new Uint8Array(kTargetLength);
  }

  target.fill(kInitialByte);

  assertThrows(() => target.setFromHex(str), SyntaxError);

  for (let i = 0; i < target.length; i++) {
    assertEquals(kInitialByte, target[i]);
  }
}

testOddLength(false, false);
testOddLength(true, false);
testOddLength(false, true);
testOddLength(true, true);
