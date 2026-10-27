// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The low bytes are ASCII 0, 9, A, F, a, f, and 5 respectively.
const code_units = [0x130, 0x139, 0x141, 0x146, 0x161, 0x166, 0x435];
const positions = [0, 7, 8, 15, 16, 23, 24, 31];

for (const code_unit of code_units) {
  for (const position of positions) {
    const input = Array(32).fill('d');
    input[position] = String.fromCharCode(code_unit);
    const string = input.join('');

    assertThrows(() => Uint8Array.fromHex(string), SyntaxError);
    assertThrows(
        () => new Uint8Array(string.length / 2).setFromHex(string),
        SyntaxError);
  }
}
