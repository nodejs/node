// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string --js-base-64

(function TestBase64FromHexWithLongInputs() {
  var arr_32 = Uint8Array.fromHex('666f6F6261726172666f6f62617261ff');
  assertSame(
      '102,111,111,98,97,114,97,114,102,111,111,98,97,114,97,255',
      arr_32.join(','));

  var arr_48 =
      Uint8Array.fromHex('666f6f6261726172666f6f62617261ff666f6f6261726172');
  assertSame(
      '102,111,111,98,97,114,97,114,102,111,111,98,97,114,97,255,102,111,111,98,97,114,97,114',
      arr_48.join(','));

  var arr_64 = Uint8Array.fromHex(
      '666f6f6261726172666f6f62617261ff666f6f62617261ff666f6f6261726172');
  assertSame(
      '102,111,111,98,97,114,97,114,102,111,111,98,97,114,97,255,102,111,111,98,97,114,97,255,102,111,111,98,97,114,97,114',
      arr_64.join(','));

  assertThrows(
      () => Uint8Array.fromHex('666f6f6261726172666f6f62617261a^'),
      SyntaxError);
  assertThrows(
      () => Uint8Array.fromHex(
          '666f6f6261726172666f6f62617261ff666f6f62617261a^'),
      SyntaxError);
  assertThrows(
      () => Uint8Array.fromHex(
          '666f6f6261726172666f6f62617261ff666f6f62617261ff666f6f62617261a^'),
      SyntaxError);

  var two_byte_32 =
      createExternalizableTwoByteString('666f6f6261726172666f6f62617261ff');
  var arr_two_byte_32 = Uint8Array.fromHex(two_byte_32);
  assertSame(
      '102,111,111,98,97,114,97,114,102,111,111,98,97,114,97,255',
      arr_two_byte_32.join(','));

  var two_byte_48 = createExternalizableTwoByteString(
      '666f6f6261726172666f6f62617261ff666f6f6261726172');
  var arr_two_byte_48 = Uint8Array.fromHex(two_byte_48);
  assertSame(
      '102,111,111,98,97,114,97,114,102,111,111,98,97,114,97,255,102,111,111,98,97,114,97,114',
      arr_two_byte_48.join(','));

  var two_byte_64 = createExternalizableTwoByteString(
      '666f6f6261726172666f6f62617261ff666f6f62617261ff666f6f6261726172');
  var arr_two_byte_64 = Uint8Array.fromHex(two_byte_64);
  assertSame(
      '102,111,111,98,97,114,97,114,102,111,111,98,97,114,97,255,102,111,111,98,97,114,97,255,102,111,111,98,97,114,97,114',
      arr_two_byte_64.join(','));

  for (let i = 0; i < 0xFF; i++) {
    const isValid = (i >= 0x30 && i <= 0x39 /* 0-9 */) ||
        (i >= 0x41 && i <= 0x46 /* A-F */) ||
        (i >= 0x61 && i <= 0x66 /* a-f */);
    const str = String.fromCharCode.apply(String, Array(32).fill(i));
    if (isValid) {
      const byte = parseInt(str.substr(0, 2), 16);
      const arr = Uint8Array.fromHex(str);
      assertSame(Array(16).fill(byte).join(','), arr.join(','));
    } else {
      assertThrows(() => Uint8Array.fromHex(str), SyntaxError);
    }
  }
})();
