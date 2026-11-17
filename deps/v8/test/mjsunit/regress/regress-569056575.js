// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for crbug.com/569056575 and crbug.com/569134276:
// FastJsonStringifier::AppendStringSWAR must use unaligned loads when reading
// 64-bit chunks of two-byte strings (e.g. when AppendStringSIMD advances past
// an escaped character at an even index leaving an odd start_index, or when
// stringifying a SlicedString with an odd offset).

(function TestTwoByteSimdToSwarUnalignedTail() {
  const escapes =
      ['"', '\\', '\n', '\u001f', '\ud800', '\udfff', '\ud83d\ude00'];
  const expectedEscapes =
      ['\\"', '\\\\', '\\n', '\\u001f', '\\ud800', '\\udfff', '\ud83d\ude00'];

  for (let e = 0; e < escapes.length; e++) {
    const esc = escapes[e];
    const exp = expectedEscapes[e];
    for (let prefixLen = 0; prefixLen <= 18; prefixLen++) {
      for (let suffixLen = 0; suffixLen <= 18; suffixLen++) {
        const s = '\u0100'.repeat(prefixLen) + esc + '\u0101'.repeat(suffixLen);
        const expected = '"' +
            '\u0100'.repeat(prefixLen) + exp + '\u0101'.repeat(suffixLen) + '"';
        assertEquals(expected, JSON.stringify(s));
        assertEquals('{"a":' + expected + '}', JSON.stringify({a: s}));
        assertEquals('[' + expected + ']', JSON.stringify([s]));
      }
    }
  }
})();

(function TestTwoByteSlicedStringUnalignedOffset() {
  const parent = 'x' +
      '\u0100'.repeat(60) + '"' +
      '\u0101'.repeat(20);
  for (let offset = 1; offset <= 8; offset++) {
    for (let len = 4; len <= 36; len++) {
      const slice = parent.substring(offset, offset + len);
      const parsed = JSON.parse(JSON.stringify(slice));
      assertEquals(slice, parsed);
    }
  }
})();
