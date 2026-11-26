// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestBase64PrototypeToHexWithLongInputs() {
  assertEquals(
      (new Uint8Array([102, 111, 111, 98, 97, 114, 111, 111])).toHex(),
      '666f6f6261726f6f');
  assertEquals(
      (new Uint8Array([102, 111, 111, 98, 97, 114, 111, 111, 111, 111]))
          .toHex(),
      '666f6f6261726f6f6f6f');
  assertEquals(
      (new Uint8Array([
        102, 111, 111, 98, 97, 114, 111, 111, 102, 111, 111, 98, 97, 114, 111,
        111
      ])).toHex(),
      '666f6f6261726f6f666f6f6261726f6f');
  assertEquals(
      (new Uint8Array([
        102, 111, 111, 98, 97, 114, 111, 111, 102, 111, 111, 98, 97, 114, 111,
        111, 111, 111
      ])).toHex(),
      '666f6f6261726f6f666f6f6261726f6f6f6f');
  assertEquals(
      (new Uint8Array([
        102, 111, 111, 98,  97,  114, 111, 111, 102, 111, 111, 98,
        97,  114, 111, 111, 102, 111, 111, 98,  97,  114, 111, 111
      ])).toHex(),
      '666f6f6261726f6f666f6f6261726f6f666f6f6261726f6f');
  assertEquals(
      (new Uint8Array([
        102, 111, 111, 98,  97,  114, 111, 111, 102, 111, 111, 98,  97,
        114, 111, 111, 102, 111, 111, 98,  97,  114, 111, 111, 111, 111
      ])).toHex(),
      '666f6f6261726f6f666f6f6261726f6f666f6f6261726f6f6f6f');

  const max_elem = 256;
  const alpha = 87;
  let array = new Uint8Array(max_elem);
  let ref = "";
  const zero = 48;

  for (let i = 0; i < max_elem; i++) {
    array[i] = i;

    let high_nibble = i >>> 4;
    let low_nibble = i & 15;

    high_nibble = high_nibble > 9 ? high_nibble + alpha : high_nibble + zero;
    low_nibble = low_nibble > 9 ? low_nibble + alpha : low_nibble + zero;
    ref = ref.concat(String.fromCodePoint(high_nibble));
    ref = ref.concat(String.fromCodePoint(low_nibble));
  }

  assertEquals(array.toHex(), ref);
})();
