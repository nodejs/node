// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-base-64

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
})();
